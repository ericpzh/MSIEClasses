#include "lab56pkg/lab56.h"
#include <vector>
#include <cmath>
#include <stack>
#include <queue>
#include <utility>
#include <set>
using namespace std;
extern ImageConverter* ic_ptr; //global pointer from the lab56.cpp

#define SPIN_RATE 20  /* Hz */

bool isReady=1;
bool pending=0;

float SuctionValue = 0.0;

bool leftclickdone = 1;
bool rightclickdone = 1;

/*****************************************************
* Functions in class:
* **************************************************/

//constructor(don't modify)
ImageConverter::ImageConverter():it_(nh_)
{
    // Subscrive to input video feed and publish output video feed
    image_sub_ = it_.subscribe("/cv_camera_node/image_raw", 1,
    	&ImageConverter::imageCb, this);
    image_pub_ = it_.advertise("/image_converter/output_video", 1);
    namedWindow(OPENCV_WINDOW);
    pub_command=nh_.advertise<ece470_ur3_driver::command>("ur3/command",10);
    sub_position=nh_.subscribe("ur3/position",1,&ImageConverter::position_callback,this);

	sub_io_states=nh_.subscribe("ur_driver/io_states",1,&ImageConverter::suction_callback,this);

	srv_SetIO = nh_.serviceClient<ur_msgs::SetIO>("ur_driver/set_io");


    driver_msg.destination=lab_invk(0.1,0.3,0.1,-45);

	//publish the point to the robot
    ros::Rate loop_rate(SPIN_RATE); // Initialize the rate to publish to ur3/command
	spincount = 0;
	driver_msg.duration = 3.0;
	pub_command.publish(driver_msg);  // publish command, but note that is possible that
										  // the subscriber will not receive this message.
	spincount = 0;
	while (isReady) { // Waiting for isReady to be false meaning that the driver has the new command
		ros::spinOnce();  // Allow other ROS functionallity to run
		loop_rate.sleep(); // Sleep and wake up at 1/20 second (1/SPIN_RATE) interval
		if (spincount > SPIN_RATE) {  // if isReady does not get set within 1 second re-publish
			pub_command.publish(driver_msg);
			ROS_INFO_STREAM("Just Published again driver_msg");
			spincount = 0;
		}
		spincount++;  // keep track of loop count
	}
	ROS_INFO_STREAM("waiting for rdy");  // Now wait for robot arm to reach the commanded waypoint.

	while(!isReady)
	{
		ros::spinOnce();
		loop_rate.sleep();
	}
	ROS_INFO_STREAM("Ready for new point");

}

//destructor(don't modify)
ImageConverter::~ImageConverter()
{
    cv::destroyWindow(OPENCV_WINDOW);
}

void ImageConverter::position_callback(const ece470_ur3_driver::positions::ConstPtr& msg)
{
	isReady=msg->isReady;
	pending=msg->pending;
}

void ImageConverter::suction_callback(const ur_msgs::IOStates::ConstPtr& msg)
{
	SuctionValue = msg->analog_in_states[0].state;
}


//subscriber callback function, will be called when there is a new image read by camera
void ImageConverter::imageCb(const sensor_msgs::ImageConstPtr& msg)
{
    cv_bridge::CvImagePtr cv_ptr;
    try
    {
      cv_ptr = cv_bridge::toCvCopy(msg, sensor_msgs::image_encodings::BGR8);
    }
    catch (cv_bridge::Exception& e)
    {
      ROS_ERROR("cv_bridge exception: %s", e.what());
      return;
    }
    // create an gray scale version of image
    Mat gray_image;
	cvtColor( cv_ptr->image, gray_image, CV_BGR2GRAY );
    // convert to black and white img, then associate objects:

// FUNCTION you will be completing
    Mat bw_image = thresholdImage(gray_image); // bw image from own function

// FUNCTION you will be completing
    Mat associate_image = associateObjects(bw_image); // find associated objects

    // Update GUI Window
    imshow("Image window", cv_ptr->image);
    imshow("gray_scale", gray_image);
    imshow("black and white", bw_image);
    imshow("associate objects", associate_image);
    waitKey(3);
    // Output some video stream
    image_pub_.publish(cv_ptr->toImageMsg());
}

/*****************************************************
	 * Function for Lab 5
* **************************************************/
// Take a grayscale image as input and return an thresholded image.
// You will implement your algorithm for calculating threshold here.
Mat ImageConverter::thresholdImage(Mat gray_img)
{
	int   totalpixels;
	Mat bw_img  = gray_img.clone(); // copy input image to a new image
    totalpixels	  = gray_img.rows*gray_img.cols;			// total number of pixels in image
	uchar graylevel; // use this variable to read the value of a pixel
	int zt=0; // threshold grayscale value

    //Histogram of the image
    vector<int> Hist(256,0);
    for(int i=0; i<totalpixels; i++){
      Hist[gray_img.data[i]]++;

    }
    /*
    vector<uchar> Hist(256,0);
    MatConstIterator_<double> it = bw_img.begin<double>(), it_end = bw_img.end<double>();
    for(; it != it_end; ++it){
      Hist[*it] ++;
    }*/
    //Prob of each grey scale
    vector<double> P(256,0.0);
    for(int i = 0; i < 256 ;i++){
    	//cout<<"Hist:"<<Hist[i]<<endl;
      P[i] = double(Hist[i])/double(totalpixels);
    }
    double sigmaObject,sigmaBackground,miuObject,miuBackground,qObject,qBackground,miu;
    miuObject = 0; miuBackground = 0; sigmaObject = 0; sigmaBackground = 0; qObject = 0; qBackground = 0;miu = 0;
    vector<double> q0(256,0.0); vector<double> q1(256,0.0);
    vector<double> miu0(256,0.0); vector<double> miu1(256,0.0);
    vector<double> sigma0(256,0.0); vector<double> sigma1(256,0.0);
    //Recurssive approach (start from grey scale of 1) (0->bg / 1->obj)
    //miu
    for(int i = 0; i < 256 ; i++){
      miu += i*P[i];
    }
    //q0/q1[1]
    for(int i = 1; i < 256 ; i++){
      qObject += Hist[i];
    }
    q0[1] = ((Hist[0]+Hist[1])/double(totalpixels));
    q1[1] = qObject/double(totalpixels);
    //miu0/miu1[1]
    for(int i = 1; i < 256 ; i++){
      miuObject += i*P[i]/q1[1];
    }
    miu0[1] = P[1]/q0[1];
    miu1[1] = miuObject;
    //for zt = 1+1 to 255+1
    for(zt = 1; zt < 255 ; zt++){
      //cout<<q0[zt]<<"--"<<q1[zt]<<"--"<<miu0[zt]<<"--"<<miu1[zt]<<endl;
      q0[zt+1] = P[zt+1] + q0[zt];
      q1[zt+1] = -P[zt+1] + q1[zt];
      miu0[zt+1] = (zt+1)*P[zt+1]/q0[zt+1]+q0[zt]/q0[zt+1]*miu0[zt];
      miu1[zt+1] = (miu-q0[zt+1]*miu0[zt+1])/(1-q0[zt+1]);

    }
    //max MiuB
    vector<double> vet;
    vet.push_back(0.0);
    double currMax = q0[1]*(miu0[1]-miu)*(miu0[1]-miu)+ q1[1]*(miu1[1]-miu)*(miu1[1]-miu);
    vet.push_back(currMax);
    for(zt = 2 ; zt < 150 ; zt++){
      double currsigmab = q0[zt]*(miu0[zt]-miu)*(miu0[zt]-miu)+ q1[zt]*(miu1[zt]-miu)*(miu1[zt]-miu);
      vet.push_back(currsigmab);
      if(currMax < currsigmab){
        currMax = currsigmab;
      }
    }
    zt = 1;
    for(int i = 1; i < 150 ; i++){
    	if(vet[i] == currMax){
    		zt = i;
    	}
    }
    //cout<<zt<<endl;
		// threshold the image
		for(int i=0; i<totalpixels; i++)
		{
			graylevel = gray_img.data[i];
			if(graylevel>zt) bw_img.data[i]= 255; // set rgb to 255 (white)
			else             bw_img.data[i]= 0; // set rgb to 0   (black)
		}
	return bw_img;
}
void ImageConverter::getNumber(Mat bw_img)
{
	int height,width; // number of rows and colums of image
	uchar pixel; //used to read pixel value of input image
	height = bw_img.rows;
	width = bw_img.cols;
	height_ = height;
	width_ = width;
    int labelnum = 1;
  int thershold = 100;
  centroid.push_back(pair<int,int>());
  //declear arr
  pixellabel = new int*[height];
  for (int i=0;i<height;i++) {
    pixellabel[i] = new int[width];
  }
  //initialize arr at<uchar>(row,col) data[row*width+col]
  for(int row=0; row<height; row++){
    for(int col=0; col<width; col++){
      if(bw_img.data[row*width+col] > 155){ // index is calculated by index = row*COLUMN_LENGTH+column
        pixellabel[row][col] = -1; //if image pixel is white label it as unvisited BG
      }else{
        pixellabel[row][col] = 0; //if image pixel is black label it as unvisited object
      }
    }
  }
  for(int row=0; row<height; row++){
    for(int col=0; col<width; col++){
      if(pixellabel[row][col] == 0){
         int orirow = row;
         int oricol = col;
         int sumx = 0;
         int sumy = 0;
  		 queue<pair<int,int> > DFS;
  		 vector<pair<int,int> > visited;
         bool flag = false;
         pixellabel[row][col] = -1;
         visited.push_back(pair<int,int>(row,col));
         while(!flag){
          //add neigbours
          //right
          if(col + 1 < width){  //if the pixel is in the picture
            if(pixellabel[row][col+1] == 0){ //if the pixel is black unvisited
              DFS.push(pair<int,int>(row,col+1)); //add to to-be-visited stack
              //pixellabel[row][col+1] = -3;
            }
          }
          //down
          if(row + 1 < height){
            if(pixellabel[row+1][col] == 0){
              DFS.push(pair<int,int>(row+1,col));
              //pixellabel[row+1][col] = -3;
            }
          }
          //left
          if(col > 0){
            if(pixellabel[row][col-1] == 0){
              DFS.push(pair<int,int>(row,col-1));
              //pixellabel[row][col-1] = -3;
            }
          }
          //up
          if(row > 0){
            if(pixellabel[row-1][col] == 0){
              DFS.push(pair<int,int>(row-1,col));
              //pixellabel[row-1][col] = -3;
            }
          }
          //pop
          if(!DFS.empty()){
            while(!DFS.empty() && pixellabel[DFS.front().first][DFS.front().second] == -1){ // get a point in stack that  hasn't been visited
              DFS.pop();
            }
            if(!DFS.empty()){
             row = DFS.front().first;
           	 col = DFS.front().second;
           	 sumx += row;
           	 sumy += col;
           	 visited.push_back(pair<int,int>(row,col)); //visit the point and add to to-be-label stack
           	 pixellabel[row][col] = -1; //label the current point as visited black pixel
           	 DFS.pop(); //next one
        	  }
          }
          if(DFS.empty()){
            flag = true; //stop when no more pixel is in the stack
          }
        }
        if(visited.size() > thershold && visited.size() < 10000){  //this block is object
          for(unsigned i = 0 ; i < visited.size(); i ++){
            pixellabel[visited[i].first][visited[i].second] = labelnum; //label object as number
          }
          labelnum++;
          row = orirow;
          col = oricol;
          sumx /= double(visited.size());
          sumy /= double(visited.size());
          centroid.push_back(pair<int,int>(sumx,sumy));
          if(sumx > 0 && sumy > 0 ){
              for(int i = sumx-10 ; i < sumx + 10; i++){
                if(i > 0 && i < height_){
                pixellabel[i][sumy] = -3;
                }
               }
              for(int i = sumy-10 ; i < sumy + 10; i++){
                if(i > 0 && i < width_){
                pixellabel[sumx][i] = -3;
                }
              }
          }
         }   
      }
    }
  }
  for(int row=0; row<height; row++){
    for(int col=0; col<width; col++){
    if(pixellabel[row][col] == 0){
      pixellabel[row][col] = -1;
    }
  }
  }
}
/*****************************************************
	 * Function for Lab 5
* **************************************************/
// Take an black and white image and find the object it it, returns an associated image with different color for each image
// You will implement your algorithm for rastering here
Mat ImageConverter::associateObjects(Mat bw_img)
{
	//initiallize the variables you will use
	int height,width; // number of rows and colums of image
	int red, green, blue; //used to assign color of each objects
	uchar pixel; //used to read pixel value of input image
	height = bw_img.rows;
	width = bw_img.cols;
    centroid.clear();
    getNumber(bw_img);
	// assign UNIQUE color to each object
	Mat associate_img = Mat::zeros( bw_img.size(), CV_8UC3 ); // function will return this image
	Vec3b color;
	for(int row=0; row<height; row++)
	{
		for(int col=0; col<width; col++)
		{
		    int temp = pixellabel[row][col];
		    if(temp >= 10)
		        temp = temp%10;
			switch ( temp  )
			{
			    case -3:
			        red = 255;
			        green = 0;
			        blue = 0;
			        break;
				case -1:
				    red    = 255;
					green  = 255;
					blue   = 255;
					break;
				case 0:
                    red    = 220;
                    green  = 102;
                    blue   = 0;
                 	break;
				case 1:
					red    = 102;
					green  = 204;
					blue   = 255;
					break;
				case 2:
					red    = 153;
					green  = 255;
					blue   = 204;
					break;
				case 3:
					red    = 255;
					green  = 255;
					blue   = 153;
					break;
				case 4:
					red    = 255;
					green  = 204;
					blue   = 153;
					break;
				case 5:
					red    = 255;
					green  = 153;
					blue   = 153;
					break;
				case 6:
					red    = 255;
					green  = 204;
					blue   = 255;
					break;
                case 7:
                    red    = 0;
                    green  = 0;
                    blue   = 102;
                    break;
                case 8:
                    red    = 153;
                    green  = 51;
                    blue   = 51;
                    break;
                case 9:
                    red    = 0;
                    green  = 102;
                    blue   = 0;
                 	break;
				default:
					red    = 0;
					green = 0;
					blue   = 0;
					break;
			}

			color[0] = blue;
			color[1] = green;
			color[2] = red;
			associate_img.at<Vec3b>(Point(col,row)) = color;
		}
	}
	//std::cout<<"size"<<centroid.size()<<endl;
	for(unsigned i = 0 ; i < centroid.size() ; i++){
        //std::cout<<"x"<<centroid[i].first<<"y"<<centroid[i].second<<std::endl;
        
        }
        //std::cout<<"-----------------------"<<std::endl;
	return associate_img;
}

/*****************************************************
	*Function for Lab 6
 * **************************************************/
 //This is a call back function of mouse click, it will be called when there's a click on the video window.
 //You will write your coordinate transformation in onClick function.
 //By calling onClick, you can use the variables calculated in the class function directly and use publisher
 //initialized in constructor to control the robot.
 //lab4 and lab3 functions can be used since it is included in the "lab4.h"
void onMouse(int event, int x, int y, int flags, void* userdata)
{
		ic_ptr->onClick(event,x,y,flags,userdata);
}
void ImageConverter::onClick(int event,int x, int y, int flags, void* userdata)
{
	// For use with Lab 6
	// If the robot is holding a block, place it at the designated row and column.
	
	  double Or = 1/2*width_;
      double Oc = 1/2*height_;
      double beta = sqrt(6*6+(434-366)*(434-366))/0.088;///0.093; 
      double theta = -PI/64-atan((6)/(434-366))+PI;
      double Tx = 310/beta;
      double Ty = 434/beta;
      double zw = 0.035; //messure height of a block
      //
      //calculation of x,y,z
      //r = beta*xc+Or
      //c = beta*yc+Oc
      //|xc|  = |cos(t) -sin(t)|  * |xw| + |Tx|
      //|yc|    |sin(t)  cos(t)|    |yw|   |Ty|
      //xcam = cos(theta)*xw-sin(theta)*yw+Tx
      //ycam = sin(theta)*xw+cos(theta)*yw+Ty
      //
	if  ( event == EVENT_LBUTTONDOWN ) //if left click, do nothing other than printing the clicked point
	{
		if (leftclickdone == 1) {
			leftclickdone = 0;  // code started
			ROS_INFO_STREAM("left click:  (" << x << ", " << y << ")");  //the point you clicked
			// put your left click code here
			int ix = x;
		    int iy = y;
			if(pixellabel[y][x] >= 0){
                y = centroid[pixellabel[y][x]].first;
                x = centroid[pixellabel[y][x]].second;
            }else{
                double dist = 99999999;
                for(size_t i = 0; i <  centroid.size() ; i++){
                    double currdist = ((centroid[i].first-iy)*(centroid[i].first-iy)+(centroid[i].second-ix)*(centroid[i].second-ix));
                    if( currdist < dist){
                        y = centroid[i].first;
                        x = centroid[i].second;
                        dist = currdist;
                    }
                }
            }
            double r = y;
            double c = x;
            double xcam = (r-Or)/beta;
            double ycam = (c-Oc)/beta;
            double xw = -(cos(theta)*Tx-cos(theta)*xcam+sin(theta)*Ty-sin(theta)*ycam);
            double yw = -(cos(theta)*Ty-cos(theta)*ycam-sin(theta)*Tx+sin(theta)*xcam);
            //
            ros::Rate loop_rate(SPIN_RATE);
            driver_msg.destination = lab_invk(xw,yw,zw+0.1,0);
            driver_msg.duration = 2;
            pub_command.publish(driver_msg);
            spincount = 0;
            while (isReady) {
              ros::spinOnce();
              loop_rate.sleep();
              if (spincount > SPIN_RATE) {
                pub_command.publish(driver_msg);
                ROS_INFO("Just Published again driver_msg");
                spincount = 0;
              }
              spincount++;
            }
            ROS_INFO("waiting for rdy");
            while (!isReady) {
              ros::spinOnce();
              loop_rate.sleep();
            }
            //
            driver_msg.destination = lab_invk(xw,yw,zw,0);
            driver_msg.duration = 2;
            pub_command.publish(driver_msg);
            spincount = 0;
            while (isReady) {
              ros::spinOnce();
              loop_rate.sleep();
              if (spincount > SPIN_RATE) {
                pub_command.publish(driver_msg);
                ROS_INFO("Just Published again driver_msg");
                spincount = 0;
              }
              spincount++;
            }
            ROS_INFO("waiting for rdy");
            while (!isReady) {
              ros::spinOnce();
              loop_rate.sleep();
            }
            //
            srv.request.fun = 1;
            srv.request.pin = 0;     // Digital Output 0
            srv.request.state = 1.0; // Set DO0 on!!!
            if (srv_SetIO.call(srv)) {
                ROS_INFO("True: Switched Suction ON");
              } else {
                ROS_INFO("False");
              }
            //
            driver_msg.destination = lab_invk(xw,yw,zw+0.1,0);
            driver_msg.duration = 2;
            pub_command.publish(driver_msg);
            spincount = 0;
            while (isReady) {
              ros::spinOnce();
              loop_rate.sleep();
              if (spincount > SPIN_RATE) {
                pub_command.publish(driver_msg);
                ROS_INFO("Just Published again driver_msg");
                spincount = 0;
              }
              spincount++;
            }
            ROS_INFO("waiting for rdy");
            while (!isReady) {
              ros::spinOnce();
              loop_rate.sleep();
            }
            //
			leftclickdone = 1; // code finished
		} else {
			ROS_INFO_STREAM("Previous Left Click not finshed, IGNORING this Click");
		}
	}
	else if  ( event == EVENT_RBUTTONDOWN )//if right click, find nearest centroid,
	{
		if (rightclickdone == 1) {  // if previous right click not finished ignore
			rightclickdone = 0;  // starting code
			ROS_INFO_STREAM("right click:  (" << x << ", " << y << ")");  //the point you clicked

			// put your right click code here
            double r = y;
            double c = x;
            double xcam = (r-Or)/beta;
            double ycam = (c-Oc)/beta;
            double xw = -(cos(theta)*Tx-cos(theta)*xcam+sin(theta)*Ty-sin(theta)*ycam);
            double yw = -(cos(theta)*Ty-cos(theta)*ycam-sin(theta)*Tx+sin(theta)*xcam);
                
                //
                ros::Rate loop_rate(SPIN_RATE);
            driver_msg.destination = lab_invk(xw,yw,zw+0.1,0);
            driver_msg.duration = 2;
            pub_command.publish(driver_msg);
            spincount = 0;
            while (isReady) {
              ros::spinOnce();
              loop_rate.sleep();
              if (spincount > SPIN_RATE) {
                pub_command.publish(driver_msg);
                ROS_INFO("Just Published again driver_msg");
                spincount = 0;
              }
              spincount++;
            }
            ROS_INFO("waiting for rdy");
            while (!isReady) {
              ros::spinOnce();
              loop_rate.sleep();
            }
            //
                //
                driver_msg.destination = lab_invk(xw,yw,zw,0);
                driver_msg.duration = 2;
                pub_command.publish(driver_msg);
                spincount = 0;
                while (isReady) {
                  ros::spinOnce();
                  loop_rate.sleep();
                  if (spincount > SPIN_RATE) {
                    pub_command.publish(driver_msg);
                    ROS_INFO("Just Published again driver_msg");
                    spincount = 0;
                  }
                  spincount++;
                }
                ROS_INFO("waiting for rdy");
                while (!isReady) {
                  ros::spinOnce();
                  loop_rate.sleep();
                }
                //
                srv.request.fun = 1;
                srv.request.pin = 0;     // Digital Output 0
                srv.request.state = 0.0; // Set DO0 off
                if (srv_SetIO.call(srv)) {
                    ROS_INFO("True: Switched Suction ON");
                  } else {
                    ROS_INFO("False");
                  }
            //
            driver_msg.destination = lab_invk(xw,yw,zw+0.10,0);
            driver_msg.duration = 2;
            pub_command.publish(driver_msg);
            spincount = 0;
            while (isReady) {
              ros::spinOnce();
              loop_rate.sleep();
              if (spincount > SPIN_RATE) {
                pub_command.publish(driver_msg);
                ROS_INFO("Just Published again driver_msg");
                spincount = 0;
              }
              spincount++;
            }
            ROS_INFO("waiting for rdy");
            while (!isReady) {
              ros::spinOnce();
              loop_rate.sleep();
            }
            //
                         //
              driver_msg.destination=lab_invk(0.1,0.3,0.1,-45);
            driver_msg.duration = 2;
            pub_command.publish(driver_msg);
            spincount = 0;
            while (isReady) {
              ros::spinOnce();
              loop_rate.sleep();
              if (spincount > SPIN_RATE) {
                pub_command.publish(driver_msg);
                ROS_INFO("Just Published again driver_msg");
                spincount = 0;
              }
              spincount++;
            }
            ROS_INFO("waiting for rdy");
            while (!isReady) {
              ros::spinOnce();
              loop_rate.sleep();
            }
            //
               


			rightclickdone = 1; // code finished
		} else {
			ROS_INFO_STREAM("Previous Right Click not finshed, IGNORING this Click");
		}
	}
}
