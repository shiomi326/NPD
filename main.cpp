#include "beanstalk.hpp"
#include "ujson.hpp"
#include "double-conversion.h"
//#include "picojson.h"
#include <iostream>
#include <assert.h>
#include <stdexcept>

#include <thread>
#include <mutex>
#include <queue>
#include <atomic>

#include "opencv2/core/core.hpp"
#include "opencv2/videoio/videoio.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/objdetect/objdetect.hpp"
#include "util.h"
#include "fixedqueue.h"

extern "C" {
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
}

using namespace std;
using namespace cv;
using namespace Beanstalk;
//using namespace picojson;


//Global
FixedQueue<Mat> buffer[4](3);
//std::queue<Mat> buffer[4];
std::mutex mtxCam[4];
std::atomic<bool> grabOn; //this is lock free


ujson::value to_json(npd_image const &b) {
    return ujson::object{   { "id", b.id },
                            { "location", b.location },
                            { "timestamp", b.timestamp },
                            { "irImage", b.irImage },
                            { "colorImage", b.colorImage } };
}

string make_json(int gate){

    //random char size
    size_t charNum = 18;
    string id = random_string(charNum);

    string location;
    if(gate == 0){
        location = "ENTRY_GATE";
    }else{
        location = "EXIT_GATE";
    }

    time_t now = time(NULL);
    struct tm *pnow = localtime(&now);
    string date = strsprintf("%04d-%02d-%02dT%02d%02d%02dZ", pnow->tm_year + 1900, pnow->tm_mon + 1, pnow->tm_mday,
                             pnow->tm_hour, pnow->tm_min, pnow->tm_sec);
    charNum = 12;
    string ir_path = "/opt/mindhive/anpr/images/" + random_string(charNum) + ".jpg";
    string color_path = "/opt/mindhive/anpr/images/" + random_string(charNum) + ".jpg";

    npd_image np_data = {id,
                         location,
                         date,
                         ir_path,
                         color_path};

    vector<npd_image> np_list{ np_data };
    ujson::value value{np_list};
    string json = to_string(value);

    return json;
}


int test_benstalk(){

    Client client("127.0.0.1", 11345);
//    Client client("127.0.0.1", 11300);
     assert(client.use("test"));
     assert(client.watch("test"));

     string json = make_json(0);
//     cout << json << std::endl;


     int id = client.put(json);
     assert(id > 0);
     cout << "put job id: " << id << endl;

     Job job;
     assert(client.reserve(job) && job);
     assert(job.id() == id);

     cout << "reserved job id: "
          << job.id()
          << " with body {" << job.body() << "}"
          << endl;

     assert(client.del(job.id()));
     cout << "deleted job id: " << job.id() << endl;


    return 0;

}





// ===========================================
// Capture 1
// ===========================================
void GrabThread(VideoCapture *cap, int camNo)
{
    Mat tmp;

    while (grabOn.load() == true) //this is lock free
    {
        //grab will wait for cam FPS
        //keep grab out of lock so that
        //idle time can be used by other threads
        *cap >> tmp; //this will wait for cam FPS

        if (tmp.empty()) continue;

        //get lock only when we have a frame
        mtxCam[camNo].lock();
        buffer[camNo].push_back(tmp);
        mtxCam[camNo].unlock();

        bool show = false;
        if (show)
        {
            int font = CV_FONT_HERSHEY_PLAIN;
            putText(tmp, "THREAD FRAME", Point(10, 15), font, 1, Scalar(0, 255, 0));
            imshow("Image thread", tmp);
            waitKey(1);    //just for imshow
        }
    }
}
/*
void GrabThread(VideoCapture *cap, int camNo)
{
    Mat tmp;

    //To know how many memory blocks will be allocated to store frames in the queue.
    //Even if you grab N frames and create N x Mat in the queue
    //only few real memory blocks will be allocated
    //thanks to std::queue and cv::Mat memory recycling
    std::map<unsigned char*, int> matMemoryCounter;
    uchar * frameMemoryAddr;

    while (grabOn.load() == true) //this is lock free
    {
        //grab will wait for cam FPS
        //keep grab out of lock so that
        //idle time can be used by other threads
        *cap >> tmp; //this will wait for cam FPS

        if (tmp.empty()) continue;

        //get lock only when we have a frame
        mtxCam[camNo].lock();
        //buffer.push(tmp) stores item by reference than avoid
        //this will create a new cv::Mat for each grab
        buffer[camNo].push(Mat(tmp.size(), tmp.type()));
        tmp.copyTo(buffer[camNo].back());
        frameMemoryAddr = buffer[camNo].front().data;
        mtxCam[camNo].unlock();
        //count how many times this memory block has been used
        matMemoryCounter[frameMemoryAddr]++;

        bool show = false;
        if (show)
        {
            int font = CV_FONT_HERSHEY_PLAIN;
            putText(tmp, "THREAD FRAME", Point(10, 15), font, 1, Scalar(0, 255, 0));
            imshow("Image thread", tmp);
            waitKey(1);    //just for imshow
        }
    }
    std::cout << std::endl <<"Camera Number:" << camNo << "Number of Mat in memory: " << matMemoryCounter.size();
}
*/
void ImageShow(const Mat &src, int camNo)
{
    if(src.empty()) return;
    putText(src, "PROC FRAME", Point(10, 15), CV_FONT_HERSHEY_PLAIN, 1, Scalar(0, 255, 0));

    string windowName = strsprintf("Image main camNo=%02d", camNo);
    imshow(windowName, src);
    waitKey(1);
}


int NP_Detection(Mat (&src)[3], int camNo){

    if(src[0].empty() || src[1].empty() || src[2].empty())
        return -1;

    //resize
    Mat img[3];
    for(int i=0; i<3; i++){
        resize(src[i], img[i], Size(), 0.7, 0.7);
    }


    Mat d1,d2,diff;
    Mat im_mask,mask;
    Mat im_gray, im_bin;

    //frame 1 and 2 diff
    absdiff(img[0], img[1], d1);
    //frame 2 and 3 diff
    absdiff(img[1], img[2], d2);
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
    int mdMinArea = 200, mdMaxArea = 1000000;

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


    //If detected motion try to detect number plate
    if(motionPoints.size() > 0){
        // Bin image
        cvtColor(img[1], im_gray,COLOR_BGR2GRAY);
        threshold(im_gray,im_bin,0,255,THRESH_BINARY | THRESH_OTSU);

        // For labeling image (※CV_32S or CV_16U)
        Mat labelImage(im_bin.size(), CV_32S);
        Mat stats;
        Mat centroids;
        int nLabels = connectedComponentsWithStats(im_bin, labelImage, stats, centroids, 8);

        //Labeling
        int minArea = 200, maxArea = 1000000;
        int minWidth = 20; int minHeight = 10;
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
            rectangle(img[1], Rect(NPEelem.x, NPEelem.y, NPEelem.width, NPEelem.height), Scalar(0, 255, 0), 3);
//            char strText[32];
//            sprintf(strText, "Detected !!");
//                putText(images[FRAME2], strText, Point(NPEelem.x,NPEelem.y-40), FONT_HERSHEY_SIMPLEX, 1.0, Scalar(0,255,0), 2, CV_AA);
//                sprintf(strText, "width=%d, height=%d", NPEelem.width, NPEelem.height);
//                putText(image2, strText, Point(NPEelem.x,NPEelem.y-80), FONT_HERSHEY_SIMPLEX, 1.0, Scalar(0,0,200), 2, CV_AA);
//                sprintf(strText, "area=%d", NPEelem.area);
//                putText(image2, strText, Point(NPEelem.x,NPEelem.y-40), FONT_HERSHEY_SIMPLEX, 1.0, Scalar(0,0,200), 2, CV_AA);

            //save image
            bool save_flag = false;
            if(save_flag){
                char buff[256];
                time_t now = time(NULL);
                struct tm *pnow = localtime(&now);
                for(int i=1; i<3; i++){
                    sprintf(buff, "/home/shiomi/QtProjects/NpDetection/detected_images/%04d%02d%02d%02d%02d%02d_c%d.jpg",
                            pnow->tm_year + 1900, pnow->tm_mon + 1, pnow->tm_mday, pnow->tm_hour, pnow->tm_min, pnow->tm_sec, i);
                    imwrite(buff, img[i]);
                }
            }
        }
        return 1;
    }

    string windowName = strsprintf("Detect camNo=%d", camNo);
    imshow(windowName, img[1]);
    waitKey(1);

    return 0;
}

int MainProc() {

    Mat frame[4];
    Mat procImg[4][3];

    VideoCapture vcap[4];
    string cameraStreamAddress[4];
    //color camera  s0=(1920*1080), s2=middle(1024*576), s1=small(640*360)
    cameraStreamAddress[0] = "rtsp://192.168.5.7:554/s2";
    cameraStreamAddress[1] = "rtsp://192.168.5.8:554/s2";
    //ir camera (1920*1080), (1280*)
    cameraStreamAddress[2] = "rtsp://192.168.5.5:8554/CH001.sdp";
    cameraStreamAddress[3] = "rtsp://192.168.5.6:8554/CH001.sdp";

    //camera open
    for(int i=0; i<4; i++){
        if(!vcap[i].open(cameraStreamAddress[i])) {
            if(i<2){
                cout << "Error opening video stream [color camera" << i+1 << "]"<< endl;
            }else{
                cout << "Error opening video stream [ir camera" << i-1 << "]"<< endl;
            }
            return -1;
        }
    }

    //sub capture
    for(int i=0; i<4; i++){
        for(int j=0; j<3; j++){
            vcap[i] >> procImg[i][j];
        }
    }

    grabOn.store(true); //set the grabbing control variable
    thread t1(GrabThread, &vcap[0], 0);        //start the grabbing thread
    thread t2(GrabThread, &vcap[1], 1);        //start the grabbing thread
    thread t3(GrabThread, &vcap[2], 2);        //start the grabbing thread
    thread t4(GrabThread, &vcap[3], 3);        //start the grabbing thread
    int bufSize;

    while (true)
    {
        for(int i=0; i<4; i++){
            mtxCam[i].lock();                //lock memory for exclusive access
            bufSize = buffer[i].get_size();
//            bufSize = buffer[i].size();      //check how many frames are waiting
            if (bufSize > 0)              //if some
            {
                //reference to buffer.front() should be valid after
                //pop because of Mat memory reference counting
                //but if content can change after unlock is better to keep a copy
                //an alternative is to unlock after processing (this will lock grabbing)

//                buffer[i].front().copyTo(frame[i]);   //get the oldest grabbed frame (queue=FIFO)
//                buffer[i].back().copyTo(frame[i]); //get the newest grabbled frame
//                ImageShow(frame[i], i);

                procImg[i][1].copyTo(procImg[i][0]);
                procImg[i][2].copyTo(procImg[i][1]);
                buffer[i].get_back().copyTo(procImg[i][2]);
//                buffer[i].back().copyTo(procImg[i][2]); //get the newest grabbled frame
                //proc
                if(i > 1){
                    NP_Detection(procImg[i], i-1);
                }

//                buffer[i].pop();    //delete
            }
            mtxCam[i].unlock();
        }


        //thread end
        if(waitKey(10) == 27){
            grabOn.store(false);
            t1.join();
            t2.join();
            t3.join();
            t4.join();
            destroyAllWindows();
            break;
        }
    }
    //release
    for(int i=0; i<4; i++){
        vcap[i].release();
    }

    return 0;
}


// ==================================================
// Capture 2
// ==================================================
void flush(VideoCapture& camera)
{
    double delay = 0;

    TickMeter meter;
//    QElapsedTimer timer;

    do
    {
        meter.start();
        camera.grab();
        meter.stop();

        delay = meter.getTimeMilli();

    }
    while (delay <= 10);
}


int capture(){

    VideoCapture col_cap[2], ir_cap[2];
    //color camera
    string colStreamAddress[2], irStreamAddress[2];
//    colStreamAddress[0] = "rtsp://192.168.5.7:554/s0";
//    colStreamAddress[1] = "rtsp://192.168.5.8:554/s0";

    colStreamAddress[0] = "rtsp://192.168.5.7:554/s2";      //s2=middle(1024*576)
    colStreamAddress[1] = "rtsp://192.168.5.8:554/s2";      //s1=small(640*360)
    //ir camera
    irStreamAddress[0] = "rtsp://192.168.5.5:8554/CH001.sdp";
    irStreamAddress[1] = "rtsp://192.168.5.6:8554/CH001.sdp";


    //camera open
    for(int i=0; i<2; i++){

//        col_cap[i].set(CV_CAP_PROP_FOURCC, CV_FOURCC('H', '2', '6', '4'));
        if(!col_cap[i].open(colStreamAddress[i])) {
            cout << "Error opening video stream [clor cam" << i+1 << "]"<< endl;
            return -1;
        }
//        col_cap[i].set(CV_CAP_PROP_FPS, 20.0);
        double fps = col_cap[i].get(CV_CAP_PROP_FPS);
        cout << fps << endl;

//        ir_cap[i].set(CV_CAP_PROP_FOURCC, CV_FOURCC('H', '2', '6', '4'));
        if(!ir_cap[i].open(irStreamAddress[i])) {
            cout << "Error opening video stream [ir cam" << i+1 << "]"<< endl;
            return -1;
        }
//        ir_cap[i].set(CV_CAP_PROP_FPS, 20.0);
        fps = ir_cap[i].get(CV_CAP_PROP_FPS);
        cout << fps << endl;
    }

    //flush
    for(int i=0; i<2; i++){
        flush(col_cap[i]);
        flush(ir_cap[i]);
    }

    Mat colImg[2], irImg[2];
    Mat rsColImg[2], rsIrImg[2];
    string colWindow[2], irWindow[2];
    colWindow[0] = "color image1";
    colWindow[1] = "color image2";
    irWindow[0] = "ir image1";
    irWindow[1] = "ir image2";

//    for(int i=0; i<2; i++){
//        namedWindow(colWindow[i],1);
//        namedWindow(irWindow[i],1);
//    }

    TickMeter meter;

    //roop
    for(;;) {

//        meter.reset();
//        meter.start();
        //capture
        for(int i=0; i<2; i++){
            col_cap[i] >> colImg[i];
            if(colImg[i].empty()) break;
            ir_cap[i] >> irImg[i];
            if(irImg[i].empty()) break;

//            if(!col_cap[i].read(colImg[i])){
//                break;
//            }
//            if(!ir_cap[i].read(irImg[i])){
//                break;
//            }
        }
//        meter.stop();
//        double milliTime =meter.getTimeMilli();
//        if(milliTime>50){
//            cout << meter.getTimeMilli() << "ms" << endl;
//        }


//        //resize
//        for(int i=0; i<2; i++){
//            resize(colImg[i], rsColImg[i], cv::Size(), 0.4, 0.4);
//            resize(irImg[i], rsIrImg[i], cv::Size(), 0.4, 0.4);
//        }

//        //Display
//        for(int i=0; i<2; i++){
//            imshow(colWindow[i],rsColImg[i]);
//            imshow(irWindow[i],rsIrImg[i]);
//        }
        for(int i=0; i<2; i++){
            imshow(colWindow[i],colImg[i]);
            imshow(irWindow[i],irImg[i]);
        }

        //break;
        //40 = 25fps
        //60 = 16.7fps
        //80 = 12.5fps
//        if(waitKey(10) == 27){
        if(waitKey(1) == 27){
            destroyAllWindows();
            break;
        }

    }


    //release
    for(int i=0; i<2; i++){
        col_cap[i].release();
        ir_cap[i].release();
    }

    return 0;

}



int main(int argc, char* argv[])
{

//    if (argc != 3)
//    {
//        cout << "Error! This program needs [FilePath]_[Num].";
//            return 0;
//    }

//    string save_dir = "/opt/mindhive/anpr/images";
//    mkDirs(save_dir);
    //    std::string sRelative = "/mindhive";
    //    std::string sBase = "/opt";
    //    mkDirs(sBase,sRelative);

//    test_benstalk();

//    lvc_capture();
//    capture();

    MainProc();

/*
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
*/

    return 0;
}
