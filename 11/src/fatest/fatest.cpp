#include <iostream>
#include <stdexcept>
#include <fstream>
#include <iterator>
#include <algorithm>
#include <iomanip>
#include <numeric>

#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>

#include "frvt11.h"
#include "util.h"

using namespace FRVT;
using namespace FRVT_11;

class ProgressBarPrinter {
public:
    ProgressBarPrinter(int total_items, int images_per_item) :
        total_items(total_items), images_per_item(images_per_item), start_time(std::chrono::high_resolution_clock::now()) {}
    
    void Print(int progress)
    {
        if (progress == 0)
        {
            std::cout << "Progress: 0% | 0/" << total_items << "\r" << std::flush;
        }
        else
        {
            int items_finished = int(progress / (images_per_item + 1));
            int percentage_finished = int(items_finished / float(total_items) * 100);

            auto finish = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double> elapsed = finish - start_time;
            double passed_time = elapsed.count();
            
            double time_per_item = passed_time / items_finished;
            times_per_item.push_back(time_per_item);
            if (times_per_item.size() > 20) times_per_item.erase(times_per_item.begin());
            time_per_item = std::accumulate(times_per_item.begin(), times_per_item.end(), 0.0) / times_per_item.size();
            
            double time_remaining = time_per_item * (total_items - items_finished);
            int minutes_remaining = int(time_remaining / 60);
            int seconds_remaining = int(time_remaining) % 60;

            std::cout
                << "Progress: "
                << percentage_finished << "% | " << items_finished << "/" << total_items
                << " | Remaining time: " << minutes_remaining << ":" << (seconds_remaining < 10 ? "0" : "") << seconds_remaining
                << " | Time per item: " << time_per_item
                << " | Time per image: " << time_per_item / double(images_per_item)
                << "\r" << std::flush;
        }
    }

    void RestartTime()
    {
        start_time = std::chrono::high_resolution_clock::now();
    }

private:
    using Time = std::chrono::time_point<std::chrono::high_resolution_clock>;
    Time start_time;
    int total_items;
    std::vector<double> times_per_item;
    int images_per_item;
};

void
InitializeImplementation(std::shared_ptr<Interface>& implPtr)
{
    implPtr->initialize("/home/administrator/nist/frvt/11/config");
}

Image
CvImageToImageData(const cv::Mat& image)
{
    std::shared_ptr<uint8_t> imageArray(new uint8_t[3 * image.rows * image.cols]);
    std::memcpy(imageArray.get(), image.data, 3 * image.rows * image.cols);

    Image imageData((uint16_t)image.cols, (uint16_t)image.rows, (uint8_t)24, imageArray, Image::Label::Unknown);
    
    return imageData;
}

double
CalculateTPR(double fpr_divider, std::vector<double>& diff_scores, std::vector<double>& same_scores)
{
    std::sort(diff_scores.begin(), diff_scores.end());
    std::reverse(diff_scores.begin(), diff_scores.end());
    int borderIndex = int(diff_scores.size() / fpr_divider);
    double borderScore = diff_scores[borderIndex];

    std::cout << "Border score: " << borderScore << " (at index " << borderIndex << ")" << std::endl;

    double tp = 0;
    for (int i = 0; i < same_scores.size(); ++i)
    {
        if (same_scores[i] > borderScore) ++tp;
    }

    return tp / same_scores.size();
}

std::vector<uint8_t>
GetTemplate(std::shared_ptr<Interface>& implPtr, std::vector<std::string> files)
{
    std::vector<Image> images;
    for (const std::string& file : files) {
        auto path = "/home/administrator/face_data/benchmarks/original/" + file;
        cv::Mat image = cv::imread(path);
        if(!image.data) throw std::runtime_error("Could not open or find the image");
        Image imageData = CvImageToImageData(image);
        images.push_back(imageData);
    }
    
    std::vector<uint8_t> features;
    std::vector<EyePair> eyeCoordinates;

    implPtr->createTemplate(images, TemplateRole::Enrollment_11, features, eyeCoordinates);

    return features;
}

void
RunVggTest(const std::string& list_path)
{
    // Read test list
    std::ifstream is(list_path);
    std::istream_iterator<std::string> start(is), end;
    std::vector<std::string> testList(start, end);

    int gallery_size = std::stoi(testList[0]);
    int pair_size = gallery_size * 2 + 1;
    std::cout << "Found gallery size: " << gallery_size << std::endl;

    // Create FRVT implementation
    auto implPtr = Interface::getImplementation();
    InitializeImplementation(implPtr);

    // Loop over pairs

    std::vector<double> same_scores;
    std::vector<double> diff_scores;

    int total_items = int((testList.size() - 1) / pair_size);

    ProgressBarPrinter progress_bar(total_items, gallery_size * 2);

    for (int i = 1, progress = 0; i < testList.size(); i = i + pair_size, progress = progress + pair_size)
    {
        if (progress == 0) progress_bar.Print(progress);

        std::vector<std::string> files1(testList.begin() + i, testList.begin() + i + gallery_size);
        std::vector<std::string> files2(testList.begin() + i + gallery_size, testList.begin() + i + gallery_size * 2);

        std::vector<uint8_t> features1 = GetTemplate(implPtr, files1);
        std::vector<uint8_t> features2 = GetTemplate(implPtr, files2);

        auto isSame = testList[i + gallery_size * 2] == "1";

        double score = 0;
        implPtr->matchTemplates(features1, features2, score);

        if (isSame) same_scores.push_back(score);
        else diff_scores.push_back(score);

        if (progress == 0) progress_bar.RestartTime();
        if (progress > 0) progress_bar.Print(progress);
    }

    // Output TPR

    std::cout << std::endl;
    std::cout << "TPR @ FPR 1:" << 10 << " = " << CalculateTPR(10, diff_scores, same_scores) << std::endl;
    std::cout << "TPR @ FPR 1:" << 100 << " = " << CalculateTPR(100, diff_scores, same_scores) << std::endl;
    std::cout << "TPR @ FPR 1:" << 1000 << " = " << CalculateTPR(1000, diff_scores, same_scores) << std::endl;
}

int
main(int argc, char* argv[])
{
    if (argc == 1) {
        std::cout << "Need test list path" << std::endl;
    }
    std::string listPath = argv[1];

    std::cout << "List path: " << listPath << std::endl;
    
    RunVggTest(listPath);

	return 0;
}