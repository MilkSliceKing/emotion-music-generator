#ifndef FACE_DETECTOR_H
#define FACE_DETECTOR_H

#include <vector>
#include <string>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing.h>
#include <dlib/opencv/cv_image.h>
#include <opencv2/opencv.hpp>

// 面部矩形区域
struct FaceRect {
    int x, y, width, height;
};

class FaceDetector {
public:
    FaceDetector();
    ~FaceDetector() = default;

    // 加载 68 关键点预测模型
    bool loadModel(const std::string& model_path);

    // 检测画面中的所有人脸
    std::vector<FaceRect> detectFaces(const cv::Mat& frame);

    // 获取单张人脸的 68 关键点
    std::vector<cv::Point> getLandmarks(const cv::Mat& frame, const FaceRect& face);

    // 在画面上绘制结果
    void drawFace(cv::Mat& frame, const FaceRect& face);
    void drawLandmarks(cv::Mat& frame, const std::vector<cv::Point>& landmarks);

private:
    dlib::frontal_face_detector detector_;
    dlib::shape_predictor predictor_;
    bool model_loaded_ = false;

    dlib::rectangle toDlibRect(const FaceRect& face);
    FaceRect fromDlibRect(const dlib::rectangle& rect);
};

#endif // FACE_DETECTOR_H
