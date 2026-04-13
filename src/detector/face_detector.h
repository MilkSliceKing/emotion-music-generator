#ifndef FACE_DETECTOR_H
#define FACE_DETECTOR_H

#include <vector>
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/dnn.hpp>

// 面部矩形区域
struct FaceRect {
    int x, y, width, height;
};

class FaceDetector {
public:
    FaceDetector();
    ~FaceDetector() = default;

    // 加载 SSD 人脸检测模型 (Caffe)
    bool loadDetector(const std::string& model_dir);

    // 加载 68 关键点预测模型 (dlib) — 可选，仅用于可视化
    bool loadLandmarkModel(const std::string& model_path);

    // 检测画面中的所有人脸 (使用 OpenCV DNN SSD)
    std::vector<FaceRect> detectFaces(const cv::Mat& frame);

    // 获取单张人脸的 68 关键点 (需要加载 landmark 模型)
    std::vector<cv::Point> getLandmarks(const cv::Mat& frame, const FaceRect& face);

    // 在画面上绘制结果
    void drawFace(cv::Mat& frame, const FaceRect& face);
    void drawLandmarks(cv::Mat& frame, const std::vector<cv::Point>& landmarks);

    // 置信度阈值 (0-1)，越高越严格
    void setConfidenceThreshold(float thresh) { confidence_threshold_ = thresh; }

private:
    // OpenCV DNN SSD 人脸检测器
    cv::dnn::Net face_net_;
    bool detector_loaded_ = false;
    float confidence_threshold_ = 0.5f;

    // dlib 关键点预测器 (可选)
    void* predictor_ = nullptr;  // dlib::shape_predictor*
    bool landmark_loaded_ = false;
};

#endif // FACE_DETECTOR_H
