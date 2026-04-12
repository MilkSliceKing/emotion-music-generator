#include "face_detector.h"
#include <iostream>

FaceDetector::FaceDetector()
    : detector_(dlib::get_frontal_face_detector())
    , model_loaded_(false)
{
}

bool FaceDetector::loadModel(const std::string& model_path) {
    try {
        dlib::deserialize(model_path) >> predictor_;
        model_loaded_ = true;
        std::cout << "[FaceDetector] 模型加载成功: " << model_path << std::endl;
        return true;
    } catch (const dlib::error& e) {
        std::cerr << "[FaceDetector] 模型加载失败: " << e.what() << std::endl;
        std::cerr << "  请下载模型:" << std::endl;
        std::cerr << "  wget http://dlib.net/files/shape_predictor_68_face_landmarks.dat.bz2" << std::endl;
        std::cerr << "  bzip2 -d shape_predictor_68_face_landmarks.dat.bz2" << std::endl;
        std::cerr << "  mv shape_predictor_68_face_landmarks.dat models/" << std::endl;
        return false;
    }
}

std::vector<FaceRect> FaceDetector::detectFaces(const cv::Mat& frame) {
    std::vector<FaceRect> faces;

    try {
        dlib::cv_image<dlib::bgr_pixel> dlib_img(frame);
        auto dets = detector_(dlib_img, 1);

        for (const auto& d : dets) {
            faces.push_back(fromDlibRect(d));
        }
    } catch (const dlib::error& e) {
        std::cerr << "[FaceDetector] 检测错误: " << e.what() << std::endl;
    }

    return faces;
}

std::vector<cv::Point> FaceDetector::getLandmarks(const cv::Mat& frame, const FaceRect& face) {
    std::vector<cv::Point> landmarks;

    if (!model_loaded_) {
        std::cerr << "[FaceDetector] 错误: 模型未加载，无法提取关键点" << std::endl;
        return landmarks;
    }

    try {
        dlib::cv_image<dlib::bgr_pixel> dlib_img(frame);
        auto shape = predictor_(dlib_img, toDlibRect(face));

        for (unsigned long i = 0; i < shape.num_parts(); ++i) {
            auto p = shape.part(i);
            landmarks.emplace_back(static_cast<int>(p.x()), static_cast<int>(p.y()));
        }
    } catch (const dlib::error& e) {
        std::cerr << "[FaceDetector] 关键点提取错误: " << e.what() << std::endl;
    }

    return landmarks;
}

void FaceDetector::drawFace(cv::Mat& frame, const FaceRect& face) {
    cv::rectangle(frame,
                  cv::Point(face.x, face.y),
                  cv::Point(face.x + face.width, face.y + face.height),
                  cv::Scalar(0, 255, 0), 2);
    cv::putText(frame, "Face",
                cv::Point(face.x, face.y - 8),
                cv::FONT_HERSHEY_SIMPLEX, 0.5,
                cv::Scalar(0, 255, 0), 1);
}

void FaceDetector::drawLandmarks(cv::Mat& frame, const std::vector<cv::Point>& landmarks) {
    // 不同面部区域用不同颜色绘制
    // 0-16: 轮廓  17-26: 眉毛  27-35: 鼻子  36-47: 眼睛  48-67: 嘴巴
    auto getColor = [](int idx) -> cv::Scalar {
        if (idx < 17) return cv::Scalar(200, 200, 200);
        if (idx < 22) return cv::Scalar(255, 150, 0);
        if (idx < 27) return cv::Scalar(255, 150, 0);
        if (idx < 36) return cv::Scalar(0, 220, 220);
        if (idx < 48) return cv::Scalar(220, 220, 0);
        return cv::Scalar(0, 0, 255);
    };

    for (size_t i = 0; i < landmarks.size(); ++i) {
        cv::circle(frame, landmarks[i], 2, getColor(static_cast<int>(i)), -1);
    }
}

dlib::rectangle FaceDetector::toDlibRect(const FaceRect& face) {
    return dlib::rectangle(
        static_cast<long>(face.x),
        static_cast<long>(face.y),
        static_cast<long>(face.x + face.width),
        static_cast<long>(face.y + face.height)
    );
}

FaceRect FaceDetector::fromDlibRect(const dlib::rectangle& rect) {
    return FaceRect{
        static_cast<int>(rect.left()),
        static_cast<int>(rect.top()),
        static_cast<int>(rect.width()),
        static_cast<int>(rect.height())
    };
}
