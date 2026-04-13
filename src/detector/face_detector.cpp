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
    for (size_t i = 0; i < landmarks.size(); ++i) {
        cv::circle(frame, landmarks[i], 2, cv::Scalar(0, 0, 255), -1);
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
