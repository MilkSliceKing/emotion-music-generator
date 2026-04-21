#include "face_detector.h"
#include <iostream>

FaceDetector::FaceDetector()
    : detector_(dlib::get_frontal_face_detector())
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

    if (!model_loaded_) return landmarks;

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

void FaceDetector::drawFace(cv::Mat& frame, const FaceRect& face,
                             const cv::Scalar& color) {
    int x = face.x, y = face.y, w = face.width, h = face.height;
    int corner = std::min({15, w / 4, h / 4});
    int thick = 3;

    // 四角装饰线
    cv::line(frame, cv::Point(x, y), cv::Point(x + corner, y), color, thick);
    cv::line(frame, cv::Point(x, y), cv::Point(x, y + corner), color, thick);

    cv::line(frame, cv::Point(x + w, y), cv::Point(x + w - corner, y), color, thick);
    cv::line(frame, cv::Point(x + w, y), cv::Point(x + w, y + corner), color, thick);

    cv::line(frame, cv::Point(x, y + h), cv::Point(x + corner, y + h), color, thick);
    cv::line(frame, cv::Point(x, y + h), cv::Point(x, y + h - corner), color, thick);

    cv::line(frame, cv::Point(x + w, y + h), cv::Point(x + w - corner, y + h), color, thick);
    cv::line(frame, cv::Point(x + w, y + h), cv::Point(x + w, y + h - corner), color, thick);
}

void FaceDetector::drawLandmarks(cv::Mat& frame, const std::vector<cv::Point>& landmarks) {
    for (size_t i = 0; i < landmarks.size(); ++i) {
        cv::circle(frame, landmarks[i], 1, cv::Scalar(230, 180, 100), -1);
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
