#include "emotion_recognizer.h"
#include <cmath>

std::string emotionToString(Emotion e) {
    switch (e) {
        case Emotion::HAPPY:     return "Happy";
        case Emotion::SAD:       return "Sad";
        case Emotion::ANGRY:     return "Angry";
        case Emotion::SURPRISED: return "Surprised";
        case Emotion::NEUTRAL:   return "Neutral";
        case Emotion::FEAR:      return "Fear";
        case Emotion::DISGUST:   return "Disgust";
        default:                 return "Unknown";
    }
}

double EmotionRecognizer::distance(const cv::Point& p1, const cv::Point& p2) {
    return std::sqrt(std::pow(p1.x - p2.x, 2.0) + std::pow(p1.y - p2.y, 2.0));
}

// 嘴巴纵横比 (MAR) — 嘴巴张开的程度
// dlib 68点: 外唇轮廓 48-59，内唇 60-67
double EmotionRecognizer::getMouthAspectRatio(const std::vector<cv::Point>& lm) {
    double vertical = distance(lm[51], lm[57]);   // 上下唇中心距离
    double horizontal = distance(lm[48], lm[54]); // 嘴角间距
    if (horizontal < 1e-6) return 0.0;
    return vertical / horizontal;
}

// 眼睛纵横比 (EAR) — 眼睛睁大程度
// 左眼: 36-41, 右眼: 42-47
double EmotionRecognizer::getEyeAspectRatio(const std::vector<cv::Point>& lm, bool left) {
    if (left) {
        double v1 = distance(lm[37], lm[41]);
        double v2 = distance(lm[38], lm[40]);
        double h  = distance(lm[36], lm[39]);
        if (h < 1e-6) return 0.0;
        return (v1 + v2) / (2.0 * h);
    } else {
        double v1 = distance(lm[43], lm[47]);
        double v2 = distance(lm[44], lm[46]);
        double h  = distance(lm[42], lm[45]);
        if (h < 1e-6) return 0.0;
        return (v1 + v2) / (2.0 * h);
    }
}

// 眉毛高度 — 眉毛相对眼睛的距离（越大表示眉毛越上扬）
// 左眉: 17-21, 左眼: 36-41; 右眉: 22-26, 右眼: 42-47
double EmotionRecognizer::getEyebrowHeight(const std::vector<cv::Point>& lm, bool left) {
    if (left) {
        double brow_y = (lm[19].y + lm[20].y + lm[21].y) / 3.0;
        double eye_y  = (lm[37].y + lm[38].y + lm[40].y + lm[41].y) / 4.0;
        return eye_y - brow_y; // y轴向下，正值=眉毛在眼睛上方
    } else {
        double brow_y = (lm[22].y + lm[23].y + lm[24].y) / 3.0;
        double eye_y  = (lm[43].y + lm[44].y + lm[46].y + lm[47].y) / 4.0;
        return eye_y - brow_y;
    }
}

// 嘴角角度 — 正值=嘴角下垂(悲伤)，负值=嘴角上扬(微笑)
double EmotionRecognizer::getMouthCornerAngle(const std::vector<cv::Point>& lm) {
    cv::Point mouth_center((lm[51].x + lm[57].x) / 2, (lm[51].y + lm[57].y) / 2);
    double left_angle  = std::atan2(mouth_center.y - lm[48].y,
                                    mouth_center.x - lm[48].x);
    double right_angle = std::atan2(mouth_center.y - lm[54].y,
                                    lm[54].x - mouth_center.x);
    return (left_angle + right_angle) / 2.0;
}

Emotion EmotionRecognizer::recognize(const std::vector<cv::Point>& landmarks) {
    if (landmarks.size() < 68) return Emotion::NEUTRAL;

    // 计算五个关键特征
    double mar       = getMouthAspectRatio(landmarks);
    double ear_left  = getEyeAspectRatio(landmarks, true);
    double ear_right = getEyeAspectRatio(landmarks, false);
    double ear_avg   = (ear_left + ear_right) / 2.0;
    double brow_left  = getEyebrowHeight(landmarks, true);
    double brow_right = getEyebrowHeight(landmarks, false);
    double brow_avg   = (brow_left + brow_right) / 2.0;
    double mouth_corner = getMouthCornerAngle(landmarks);

    // ---- 基于阈值的规则分类 ----
    // 优先级从高到低，特征越明显的越先判断

    // 惊讶: 嘴巴大张 + 眼睛睁大 + 眉毛上扬
    if (mar > 0.55 && ear_avg > 0.33 && brow_avg > 12.0) {
        return Emotion::SURPRISED;
    }

    // 开心: 嘴角上扬 + 嘴巴适度张开
    if (mouth_corner < -0.08 && mar > 0.25) {
        return Emotion::HAPPY;
    }

    // 愤怒: 眉毛下压 + 嘴巴紧闭
    if (brow_avg < 5.0 && mar < 0.2) {
        return Emotion::ANGRY;
    }

    // 恐惧: 嘴巴张开 + 眉毛上扬
    if (mar > 0.4 && brow_avg > 10.0) {
        return Emotion::FEAR;
    }

    // 悲伤: 嘴角下垂
    if (mouth_corner > 0.15) {
        return Emotion::SAD;
    }

    // 厌恶: 嘴角下垂 + 眉毛微压
    if (mouth_corner > 0.05 && brow_avg < 7.0) {
        return Emotion::DISGUST;
    }

    return Emotion::NEUTRAL;
}
