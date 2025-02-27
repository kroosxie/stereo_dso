/**
* This file is part of DSO.
* 
* Copyright 2016 Technical University of Munich and Intel.
* Developed by Jakob Engel <engelj at in dot tum dot de>,
* for more information see <http://vision.in.tum.de/dso>.
* If you use this code, please cite the respective publications as
* listed on the above website.
*
* DSO is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* (at your option) any later version.
*
* DSO is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with DSO. If not, see <http://www.gnu.org/licenses/>.
*/


#pragma once
#include "boost/thread.hpp"
#include "util/MinimalImage.h"
#include "IOWrapper/Output3DWrapper.h"



#include "FullSystem/HessianBlocks.h"
#include "util/FrameShell.h"
#include "util/settings.h"

namespace dso
{

class FrameHessian;
class CalibHessian;
class FrameShell;


namespace IOWrap
{

class SampleOutputWrapper : public Output3DWrapper
{
public:
        inline SampleOutputWrapper()
        {
            // added by xjc
            numPCL = 0;
            isSavePCL = true;
            isPCLfileClose = false;            

            // refresh pclFile first
            pclFile.open(strTmpFileName); 
            pclFile.close();         

            printf("OUT: Created SampleOutputWrapper\n");
        }

        virtual ~SampleOutputWrapper()
        {
            // added by xjc
            if (pclFile.is_open())
            {
                pclFile.close();
            }

            printf("OUT: Destroyed SampleOutputWrapper\n");
        }

        virtual void publishGraph(const std::map<long,Eigen::Vector2i> &connectivity)
        {
            printf("OUT: got graph with %d edges\n", (int)connectivity.size());

            int maxWrite = 5;

            for(const std::pair<long,Eigen::Vector2i> &p : connectivity)
            {
                int idHost = p.first>>32;
                int idTarget = p.first & 0xFFFFFFFF;
                printf("OUT: Example Edge %d -> %d has %d active and %d marg residuals\n", idHost, idTarget, p.second[0], p.second[1]);
                maxWrite--;
                if(maxWrite==0) break;
            }
        }



        /* virtual void publishKeyframes( std::vector<FrameHessian*> &frames, bool final, CalibHessian* HCalib)
        {
            for(FrameHessian* f : frames)
            {
                printf("OUT: KF %d (%s) (id %d, tme %f): %d active, %d marginalized, %d immature points. CameraToWorld:\n",
                       f->frameID,
                       final ? "final" : "non-final",
                       f->shell->incoming_id,
                       f->shell->timestamp,
                       (int)f->pointHessians.size(), (int)f->pointHessiansMarginalized.size(), (int)f->immaturePoints.size());
                std::cout << f->shell->camToWorld.matrix3x4() << "\n";


                int maxWrite = 5;
                for(PointHessian* p : f->pointHessians)
                {
                    printf("OUT: Example Point x=%.1f, y=%.1f, idepth=%f, idepth std.dev. %f, %d inlier-residuals\n",
                           p->u, p->v, p->idepth_scaled, sqrt(1.0f / p->idepth_hessian), p->numGoodResiduals );
                    maxWrite--;
                    if(maxWrite==0) break;
                }
            }

        */

       // added by xjc
       virtual void publishKeyframes( std::vector<FrameHessian*> &frames, bool final, CalibHessian* HCalib) override
        {
            float fx, fy, cx, cy;
            float fxi, fyi, cxi, cyi;
            //float colorIntensity = 1.0f;
            fx = HCalib->fxl();
            fy = HCalib->fyl();
            cx = HCalib->cxl();
            cy = HCalib->cyl();
            fxi = 1 / fx;
            fyi = 1 / fy;
            cxi = -cx / fx;
            cyi = -cy / fy;

            if (final)
            {
                for (FrameHessian* f : frames)
                {
                    if (f->shell->poseValid)
                    {
                        // CameraToWorld Matrix (坐标系变换矩阵)
                        auto const& m = f->shell->camToWorld.matrix3x4();

                        // use only marginalized points.
                        auto const& points = f->pointHessiansMarginalized;

                        /*
                        // 定义旋转角度（角度转换为弧度）
                        double angleDegrees_x = 73.0;
                        double angleRadians_x = angleDegrees_x * M_PI / 180.0;
                        double angleDegrees_y = -3;
                        double angleRadians_y = angleDegrees_y * M_PI / 180.0;

                        // 创建的AngleAxis对象
                        Eigen::AngleAxisd rotationAngleAxis_x(angleRadians_x, Eigen::Vector3d::UnitX()); // 绕x轴旋转
                        Eigen::AngleAxisd rotationAngleAxis_y(angleRadians_y, Eigen::Vector3d::UnitY()); // 绕y轴旋转
                        Eigen::AngleAxisd rotationAngleAxis_x_invert(M_PI, Eigen::Vector3d::UnitX()); // 绕x轴翻转


                        // 使用AngleAxis对象构造旋转矩阵
                        Eigen::Matrix3d rotationMatrix = rotationAngleAxis_x_invert.toRotationMatrix() * rotationAngleAxis_y.toRotationMatrix() * rotationAngleAxis_x.toRotationMatrix();

                        // 打印旋转矩阵
                        std::cout << "旋转矩阵:" << std::endl;
                        std::cout << rotationMatrix << std::endl;
                        */

                        pclFile.open(strTmpFileName, std::ios::out | std::ios::app);
                        
                        for (auto const* p : points)
                        {
                            float depth = 1.0f / p->idepth;
                            auto const x = (p->u * fxi + cxi) * depth;
                            auto const y = (p->v * fyi + cyi) * depth;
                            auto const z = depth;
                            // auto const z = depth * (1 + 2 * fxi * (rand()/(float)RAND_MAX-0.5f));  //KeyFrameDisplay.cpp使用的计算方式


                            Eigen::Vector4d camPoint(x, y, z, 1.f);
                            // Eigen::Vector3d worldPoint = rotationMatrix * m * camPoint;
                            Eigen::Vector3d worldPoint = m * camPoint;

                            if (isSavePCL && pclFile.is_open())
                            {
                                isWritePCL = true;

                                // pclFile << worldPoint[0] << " " << worldPoint[1] << " " << worldPoint[2] << "\n";
                                pclFile << worldPoint[0] << " " << worldPoint[2] << " " << -worldPoint[1] << "\n";  // 手动调整x,y,z轴顺序

                                printf("[%d] Point Cloud Coordinate> X: %.2f, Y: %.2f, Z: %.2f\n",
                                         numPCL,
                                         worldPoint[0],
                                         worldPoint[1],
                                         worldPoint[2]);                             
                                numPCL++;
                                isWritePCL = false;
                            }
                            else
                            {
                                if (!isPCLfileClose)
                                {
                                    if (pclFile.is_open())
                                    {
                                        pclFile.flush();
                                        pclFile.close();
                                        isPCLfileClose = true;
                                    }
                                }
                            }


                        }

                        while(1)
                        {
                            // std::ofstream saveTmpFile(strSaveFileName);
                            pclFile.close();
                            std::ofstream saveTmpFile(strSaveTmpFileName);
                            saveTmpFile << std::string("# .PCD v.6 - Point Cloud Data file format\n");
                            saveTmpFile << std::string("FIELDS x y z\n");
                            saveTmpFile << std::string("SIZE 4 4 4\n");
                            saveTmpFile << std::string("TYPE F F F\n");
                            saveTmpFile << std::string("COUNT 1 1 1\n");
                            saveTmpFile << std::string("WIDTH ") << numPCL << std::string("\n");
                            saveTmpFile << std::string("HEIGHT 1\n");
                            saveTmpFile << std::string("#VIEWPOINT 0 0 0 1 0 0 0\n");
                            saveTmpFile << std::string("POINTS ") << numPCL << std::string("\n");
                            saveTmpFile << std::string("DATA ascii\n");

                            std::ifstream pclReadFile(strTmpFileName);

                            while (!pclReadFile.eof())
                            {                        
                                saveTmpFile.put(pclReadFile.get());                        
                            }

                            saveTmpFile.close();
                            pclReadFile.close();
                            // std::ofstream pclFile(strTmpFileName, std::ios::out | std::ios::app);

                            printf(" Tmp PCL File is saved.\n");

                            break;

                        }

                    }
                }
            }

        }

        // added by xjc 2024.10.10 & 2025.2.18
       virtual void publishLiveKeyframe( FrameHessian* frame, bool final, CalibHessian* HCalib) override
        {
            float fx, fy, cx, cy;
            float fxi, fyi, cxi, cyi;
            //float colorIntensity = 1.0f;
            fx = HCalib->fxl();
            fy = HCalib->fyl();
            cx = HCalib->cxl();
            cy = HCalib->cyl();
            fxi = 1 / fx;
            fyi = 1 / fy;
            cxi = -cx / fx;
            cyi = -cy / fy;

            if (final)
            {
                FrameHessian* f = frame;
            
                if (f->shell->poseValid)
                {
                    // CameraToWorld Matrix (坐标系变换矩阵)
                    auto const& m = f->shell->camToWorld.matrix3x4();

                    // use only marginalized points.
                    auto const& points = f->pointHessiansMarginalized;
                    // auto const& points = f->pointHessiansMarginalized;
                    auto const numPCL_Realtime = points.size();
                    std::cout << "The size of points is: " << numPCL_Realtime << std::endl;  // result : 0 ? why ?

                    std::cout << "begin real-time pclfile" << std::endl;

                    // add by xjc 2024.10.10
                    std::ofstream pclFile_realtime(strRealtimeFileName, std::ofstream::trunc);  // 每次写入时都清空文件内容
                    pclFile_realtime << std::string("# .PCD v.6 - Point Cloud Data file format\n");
                    pclFile_realtime << std::string("FIELDS x y z\n");
                    pclFile_realtime << std::string("SIZE 4 4 4\n");
                    pclFile_realtime << std::string("TYPE F F F\n");
                    pclFile_realtime << std::string("COUNT 1 1 1\n");
                    pclFile_realtime << std::string("WIDTH ") << numPCL_Realtime << std::string("\n");  // Error: points.size = 0
                    pclFile_realtime << std::string("HEIGHT 1\n");
                    pclFile_realtime << std::string("#VIEWPOINT 0 0 0 1 0 0 0\n");
                    pclFile_realtime << std::string("POINTS ") << numPCL_Realtime << std::string("\n");  // Error: points.size = 0
                    pclFile_realtime << std::string("DATA ascii\n");
    
                    for (auto const* p : points)  // 没进去
                    {
                        float depth = 1.0f / p->idepth;
                        auto const x = (p->u * fxi + cxi) * depth;
                        auto const y = (p->v * fyi + cyi) * depth;
                        auto const z = depth;

                        Eigen::Vector4d camPoint(x, y, z, 1.f);
                        Eigen::Vector3d worldPoint = m * camPoint;

                        printf("Live Key-Frame Point Cloud Coordinate> X: %.2f, Y: %.2f, Z: %.2f\n",
                                    worldPoint[0],
                                    worldPoint[1],
                                    worldPoint[2]);
                        
                        if (isSavePCL && pclFile_realtime.is_open())
                            {
                                isWritePCL = true;

                                // pclFile << worldPoint[0] << " " << worldPoint[1] << " " << worldPoint[2] + 2.5 << "\n";  // 沿z轴方向上下平移
                                // pclFile << worldPoint[0] << " " << worldPoint[1] << " " << worldPoint[2] << "\n"; 
                                pclFile_realtime << worldPoint[0] << " " << worldPoint[2] << " " << -worldPoint[1] << "\n";  // 手动调整x,y,z轴顺序

                                isWritePCL = false;
                            }
                            else
                            {
                                if (!isPCLfileClose)
                                {
                                    if (pclFile_realtime.is_open())
                                    {
                                        pclFile_realtime.flush();
                                        pclFile_realtime.close();
                                        isPCLfileClose = true;
                                    }
                                }
                            }

                    }

                    pclFile_realtime.flush();
                    pclFile_realtime.close();
                }
            
            }

       }

        virtual void publishCamPose(FrameShell* frame, CalibHessian* HCalib)
        {
            printf("OUT: Current Frame %d (time %f, internal ID %d). CameraToWorld:\n",
                   frame->incoming_id,
                   frame->timestamp,
                   frame->id);
            std::cout << frame->camToWorld.matrix3x4() << "\n";
        }


        virtual void pushLiveFrame(FrameHessian* image)
        {
            // can be used to get the raw image / intensity pyramid.
        }

        virtual void pushDepthImage(MinimalImageB3* image)
        {
            // can be used to get the raw image with depth overlay.
        }
        virtual bool needPushDepthImage()
        {
            return false;
        }

        virtual void pushDepthImageFloat(MinimalImageF* image, FrameHessian* KF )
        {
            printf("OUT: Predicted depth for KF %d (id %d, time %f, internal frame-ID %d). CameraToWorld:\n",
                   KF->frameID,
                   KF->shell->incoming_id,
                   KF->shell->timestamp,
                   KF->shell->id);
            std::cout << KF->shell->camToWorld.matrix3x4() << "\n";

            int maxWrite = 5;
            for(int y=0;y<image->h;y++)
            {
                for(int x=0;x<image->w;x++)
                {
                    if(image->at(x,y) <= 0) continue;

                    printf("OUT: Example Idepth at pixel (%d,%d): %f.\n", x,y,image->at(x,y));
                    maxWrite--;
                    if(maxWrite==0) break;
                }
                if(maxWrite==0) break;
            }
        }
 
        std::ofstream pclFile;


};



}



}
