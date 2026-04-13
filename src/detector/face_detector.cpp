#include "face_detector.h"
#include <iostream>
#include <algorithm>

#ifdef HAS_DLIB
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing.h>
#include <dlib/opencv/cv_image.h>
#endif

FaceDetector::FaceDetector() = default;

bool FaceDetector::loadDetector(const std::string& model_dir) {
    std::string prototxt = model_dir + "/deploy.prototxt";
    std::string caffemodel = model_dir + "/res10_300x300_ssd_iter_140000.caffemodel";

    try {
        face_net_ = cv::dnn::readNetFromCaffe(prototxt, caffemodel);
        face_net_.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        face_net_.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        detector_loaded_ = true;
        std::cout << "[FaceDetector] SSD 模型加载成功" << std::endl;
        return true;
    } catch (const cv::Exception& e) {
        std::cerr << "[FaceDetector] SSD 模型加载失败: " << e.what() << std::endl;
        return false;
    }
}

bool FaceDetector::loadLandmarkModel(const std::string& model_path) {
#ifdef HAS_DLIB
    try {
        auto* pred = new dlib::shape_predictor();
        dlib::deserialize(model_path) >> *pred;
        predictor_ = pred;
        landmark_loaded_ = true;
        std::cout << "[FaceDetector] Landmark 模型加载成功: " << model_path << std::endl;
        return true;
    } catch (const dlib::error& e) {
        std::cerr << "[FaceDetector] Landmark 模型加载失败: " << e.what() << std::endl;
        return false;
    }
#else
    std::cerr << "[FaceDetector] 编译时未启用 dlib，无法使用 landmark 功能" << std::endl;
    return false;
#endif
}

std::vector<FaceRect> FaceDetector::detectFaces(const cv::Mat& frame) {
    std::vector<FaceRect> faces;

    if (!detector_loaded_) return faces;

    // 构造输入 blob: 300x300, BGR, 缩放因子 1.0, 不减均值
    cv::Mat blob = cv::dnn::blobFromImage(frame, 1.0, cv::Size(300, 300),
                                           cv::Scalar(104.0, 177.0, 123.0));

    face_net_.setInput(blob);
    cv::Mat detections = face_net_.forward();

    // 解析检测结果
    // detections 形状: [1, 1, N, 7]，每行: [image_id, label, confidence, x1, y1, x2, y2]
    for (int i = 0; i < detections.size[2]; ++i) {
        float confidence = detections.at<float>(0, 0, i, 2);

        if (confidence < confidence_threshold_) continue;

        int x1 = static_cast<int>(detections.at<float>(0, 0, i, 3) * frame.cols);
        int y1 = static_cast<int>(detections.at<float>(0, 0, i, 4) * frame.rows);
        int x2 = static_cast<int>(detections.at<float>(0, 0, i, 5) * frame.cols);
        int y2 = static_cast<int>(detections.at<float>(0, 0, i, 6) * frame.rows);

        // 边界检查
        x1 = std::max(0, x1);
        y1 = std::max(0, y1);
        x2 = std::min(frame.cols - 1, x2);
        y2 = std::min(frame.rows - 1, y2);

        faces.push_back(FaceRect{x1, y1, x2 - x1, y2 - y1});
    }

    return faces;
}

std::vector<cv::Point> FaceDetector::getLandmarks(const cv::Mat& frame, const FaceRect& face) {
    std::vector<cv::Point> landmarks;

    if (!landmark_loaded_ || !predictor_) return landmarks;

#ifdef HAS_DLIB
    try {
        auto* pred = static_cast<dlib::shape_predictor*>(predictor_);

        dlib::cv_image<dlib::bgr_pixel> dlib_img(frame);
        dlib::rectangle rect(face.x, face.y,
                             face.x + face.width, face.y + face.height);

        auto shape = pred->operator()(dlib_img, rect);

        for (unsigned long i = 0; i < shape.num_parts(); ++i) {
            auto p = shape.part(i);
            landmarks.emplace_back(static_cast<int>(p.x()), static_cast<int>(p.y()));
        }
    } catch (const dlib::error& e) {
        std::cerr << "[FaceDetector] Landmark 提取错误: " << e.what() << std::endl;
    }
#endif

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
