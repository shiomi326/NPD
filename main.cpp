#include <iostream>
#include "opencv2/core/core.hpp"
#include "opencv2/videoio/videoio.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/objdetect/objdetect.hpp"
#include "util.h"

using namespace std;
using namespace cv;



int main(int argc, char *argv[])
{
    VideoCapture vcap;
//    const string videoStreamAddress = "rtsp://192.168.5.8:554/s0";
    const string videoStreamAddress = "rtsp://192.168.5.6:8554/CH001.sdp";

    if(!vcap.open(videoStreamAddress)) {
        cout << "Error opening video stream or file" << endl;
        return -1;
    }



    Mat frame; // for full color
    Mat d1,d2,diff;
    Mat im_mask,mask;
    Mat im_gray, im_bin;

    //make display windows
    namedWindow("in");
//    namedWindow("out");


    //Get 3 Frame Images
    Mat images[3];
    for(int i=0; i<3; i++){
        if(!vcap.read(frame)) return -1;
        frame.copyTo(images[i]);
//        images[i] = Mat::ones(frame.rows/2, frame.cols/2, CV_8UC3);
//        resize(frame, images[i], images[i].size());
    }


    //for save video
//    int fourcc   = VideoWriter::fourcc('X', 'V', 'I', 'D');
//    double fps   = 30.0;
//    bool isColor = true;
//    VideoWriter writer("/home/shiomi/QtProjects/NpDetection/detected_images/videofile.avi", fourcc, fps, images[FRAME2].size(), isColor);
//    if (!writer.isOpened())
//        return -1;


    for(;;) {

        //frame 1 and 2 diff
        absdiff(images[FRAME1], images[FRAME2], d1);
        //frame 2 and 3 diff
        absdiff(images[FRAME2], images[FRAME3], d2);
        //bitwise d1 and d2
        bitwise_and(d1,d2,diff);


        //if over thresold=1 other=0 input to mask
        threshold(diff,mask,5,1,THRESH_BINARY);
        //mask 1(True)=white(0)、0(False)=balck(255) input to im_mask
        threshold(mask,im_mask,0,255,THRESH_BINARY);
        //remove noise pixsels、size=5
        medianBlur(im_mask,im_mask,5);  //CV_8UC3

        // Get Gray and Bin image
        cvtColor(im_mask, im_gray,COLOR_BGR2GRAY);
        threshold(im_gray,im_bin,0,255,THRESH_BINARY | THRESH_OTSU);

        // Labelling
        Mat mdLabelImage(im_bin.size(), CV_32S);
        Mat mdStats;
        Mat mdCentroids;
        int mdMinArea = 600, mdMaxArea = 1000000;

        int nLabels = connectedComponentsWithStats(im_bin, mdLabelImage, mdStats, mdCentroids, 8);
        vector<Point> motionPoints;

        for (int i = 1; i < nLabels; ++i) {
            int *param = mdStats.ptr<int>(i);
            int mdArea = param[ConnectedComponentsTypes::CC_STAT_AREA];
            int mdHeight = param[ConnectedComponentsTypes::CC_STAT_HEIGHT];
            int mdWidth = param[ConnectedComponentsTypes::CC_STAT_WIDTH];
            if( mdArea > mdMinArea && mdArea < mdMaxArea && mdHeight < mdWidth ){
//            if( mdArea > mdMinArea && mdArea < mdMaxArea ){
                double *cparam = mdCentroids.ptr<double>(i);
                Point tempPos;
                tempPos.x = static_cast<int>(cparam[0]);
                tempPos.y = static_cast<int>(cparam[1]);
                motionPoints.push_back(tempPos);
            }
        }


//        int whiteArea = countNonZero(im_bin);
//        if(whiteArea > 500){
//        if(motionPoints.size() > 0){

//            vector<Point>::iterator it = motionPoints.begin();
//            while (it != motionPoints.end()) {
//                Point pos = *it;
//                circle(images[0],pos, 3, Scalar(0, 0, 255), -1);
//                ++it;
//            }

//            char strText[32];
//            sprintf(strText, "Motion detected !!");
//            putText(image2, strText, Point(10,10), FONT_HERSHEY_SIMPLEX, 1.0, Scalar(0,0,200), 2, CV_AA);

//            //save image
//            char buff[] = "";
//            time_t now = time(NULL);
//            struct tm *pnow = localtime(&now);
//            sprintf(buff, "/home/shiomi/QtProjects/NpDetection/detected_images/%04d%02d%02d%02d%02d%02d.jpg", pnow->tm_year + 1900, pnow->tm_mon + 1, pnow->tm_mday,
//                pnow->tm_hour, pnow->tm_min, pnow->tm_sec);
//            imwrite(buff, image2);
//        }

        //If detected motion try to detect number plate
        if(motionPoints.size() > 0){
            // Bin image
            cvtColor(images[FRAME2], im_gray,COLOR_BGR2GRAY);
            threshold(im_gray,im_bin,0,255,THRESH_BINARY | THRESH_OTSU);

            // For labeling image (※CV_32S or CV_16U)
            Mat labelImage(im_bin.size(), CV_32S);
            Mat stats;
            Mat centroids;
            int nLabels = connectedComponentsWithStats(im_bin, labelImage, stats, centroids, 8);

            //Labeling
            int minArea = 400, maxArea = 1000000;
            int minWidth = 50; int minHeight = 25;
            double aspect_ratio = 360.0/125.0;  //NZ number plate size (mm)
            double diffRatio = 1000;

            lblElement NPEelem;
            bool isDetect=false;


            //Search
            for (int i = 1; i < nLabels; ++i) {
                //finf most big area
                int *param = stats.ptr<int>(i);
                int x = param[ConnectedComponentsTypes::CC_STAT_LEFT];
                int y = param[ConnectedComponentsTypes::CC_STAT_TOP];
                int width = param[ConnectedComponentsTypes::CC_STAT_WIDTH];
                int height = param[ConnectedComponentsTypes::CC_STAT_HEIGHT];
                double ratio = (double)width / (double)height;
                int area = param[ConnectedComponentsTypes::CC_STAT_AREA];

                bool isGoodCandidate = true;
                //Inside check
                if(x <= 0 || (x+width) >= (int)im_gray.cols
                        || y <= 0 || (y+height) >= (int)im_gray.rows){
                     isGoodCandidate = false;
                }

                //Size check
                if(area < minArea || area > maxArea ||
                        width < (int)(1.5*(double)height) || width > (int)(4.0*(double)height) ||
                        width < minWidth || height < minHeight){
                    isGoodCandidate = false;
                }

                if( isGoodCandidate ){
                    // Check the detect area is include motionPoints
                    bool isInsidePoint = false;
                    vector<Point>::iterator it = motionPoints.begin();
                    while (it != motionPoints.end()) {
                        Point pos = *it;
                        if( pos.x > x && pos.x < x+width
                                && pos.y > y && pos.y < y+height){
                            isInsidePoint = true;
                            break;
                        }
                        ++it;
                    }
                    if(isInsidePoint && diffRatio > fabs(aspect_ratio - ratio)){
                        NPEelem.area = area;
                        NPEelem.x = x;
                        NPEelem.y = y;
                        NPEelem.width = width;
                        NPEelem.height = height;
                        // get center of gravity
                        double *cparam = centroids.ptr<double>(i);
                        NPEelem.centerX = static_cast<float>(cparam[0]);
                        NPEelem.centerY = static_cast<float>(cparam[1]);
                        diffRatio = fabs(aspect_ratio - ratio);
                        isDetect = true;
                    }
                }
            }

            //draw && save
            if(isDetect){

                //disp detect area
                rectangle(images[FRAME2], Rect(NPEelem.x, NPEelem.y, NPEelem.width, NPEelem.height), Scalar(0, 255, 0), 3);
                char strText[32];
                sprintf(strText, "Detected !!");
//                putText(images[FRAME2], strText, Point(NPEelem.x,NPEelem.y-40), FONT_HERSHEY_SIMPLEX, 1.0, Scalar(0,255,0), 2, CV_AA);
//                sprintf(strText, "width=%d, height=%d", NPEelem.width, NPEelem.height);
//                putText(image2, strText, Point(NPEelem.x,NPEelem.y-80), FONT_HERSHEY_SIMPLEX, 1.0, Scalar(0,0,200), 2, CV_AA);
//                sprintf(strText, "area=%d", NPEelem.area);
//                putText(image2, strText, Point(NPEelem.x,NPEelem.y-40), FONT_HERSHEY_SIMPLEX, 1.0, Scalar(0,0,200), 2, CV_AA);

                //save image
                char buff[] = "";
                time_t now = time(NULL);
                struct tm *pnow = localtime(&now);
                for(int i=0; i<3; i++){
                    sprintf(buff, "/home/shiomi/QtProjects/NpDetection/detected_images/%04d%02d%02d%02d%02d%02d_c%d.jpg",
                            pnow->tm_year + 1900, pnow->tm_mon + 1, pnow->tm_mday, pnow->tm_hour, pnow->tm_min, pnow->tm_sec, i);
                    imwrite(buff, images[i]);
                }
            }
        }

        //save video stream
//        writer << images[FRAME2];


        //Display
        //        imshow("in",frame);
        imshow("in",images[FRAME2]);
//        imshow("out",im_mask);


        //get new flame and shift images
        // New << image3 << image2 << image1 << Old
        images[FRAME2].copyTo(images[FRAME1]);
        images[FRAME3].copyTo(images[FRAME2]);
        if(!vcap.read(frame)) {
            waitKey();
        }
//        resize(frame, images[FRAME3], images[FRAME3].size());
        frame.copyTo(images[FRAME3]);

        //break;
        if(waitKey(1) >= 0){
            destroyAllWindows();
            break;
        }
    }

    //release
    vcap.release();
    return 0;
}
