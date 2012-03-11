#include "../include/phalange.h"
#include <geometry_msgs/PoseStamped.h>
#include <Eigen/Core>
#include <tf_conversions/tf_eigen.h>



Phalange::Phalange(tf::Transform const & L_T_Cref,Model const & model, std::string const & sensor_topic) : L_T_Cref_(L_T_Cref),ct_pos_(new tf::Vector3), ct_ori_(new tf::Quaternion),
													   model_(new Model(model)), obj_geom_("default"),touching_(false)
{

  contacts_subs_=nh_.subscribe<gazebo_msgs::ContactsState>(sensor_topic, 1, &Phalange::listenContacts, this);
  ct_pose_pub_=nh_.advertise<geometry_msgs::PoseStamped>(model_->name_ + "/contact_pose",1);
  set_pose_srv_ = nh_.advertiseService(model_->name_ + "/set_ref_contact_pose",&Phalange::setPose,this);

}
//---------------------------------------------------------------------
Phalange::~Phalange()
{
  delete ct_pos_;
  delete ct_ori_;
  delete model_;
}
//---------------------------------------------------------------------
void Phalange::listenContacts(const gazebo_msgs::ContactsState::ConstPtr& cts_st)
{
  geometry_msgs::PoseStamped ct_ps;
  ct_ps.header.seq=cts_st->header.seq;
  ct_ps.header.stamp=cts_st->header.stamp;
  ct_ps.header.frame_id=model_->frame_id_;

  lock_.lock();

  for(unsigned int i=0; i < cts_st->states.size();i++)
    if((cts_st->states[i].geom1_name == model_->geom_) && (cts_st->states[i].geom2_name == obj_geom_))     
      {
	(*ct_pos_)=averageVectors(cts_st->states[i].contact_positions);
        (*ct_ori_)=projectPose(averageVectors(cts_st->states[i].contact_normals));

        tf::pointTFToMsg(*ct_pos_,ct_ps.pose.position);
        tf::quaternionTFToMsg(*ct_ori_,ct_ps.pose.orientation);
        ct_pose_pub_.publish(ct_ps);
        touching_=true;
        lock_.unlock();
        return;
      }    
 
touching_=false;

(*ct_pos_)=L_T_Cref_.getOrigin();
(*ct_ori_)=L_T_Cref_.getRotation();
ct_ori_->normalize();

// Eigen::Vector3d p;
// Eigen::Vector3d a;
// tf::VectorTFToEigen(*ct_pos_, p);
// tf::VectorTFToEigen(ct_ori_->getAxis(), a);
// std::cout<<"position: "<<  p.transpose()<<std::endl;
// std::cout<<"quaternion: "<<a.transpose()<<" "<<ct_ori_->getAngle()<<std::endl;


tf::pointTFToMsg(*ct_pos_,ct_ps.pose.position);
tf::quaternionTFToMsg(*ct_ori_,ct_ps.pose.orientation);
ct_pose_pub_.publish(ct_ps);
lock_.unlock();
}
//---------------------------------------------------------------------
bool Phalange::setTargetObjGeom(std::string const & obj_geom)
{
  if(obj_geom.empty())
    {
      ROS_ERROR("Invalid traget object geometry");
	return false;
    }

  lock_.lock();
  obj_geom_=obj_geom;
  lock_.unlock();
  return true;
}
//---------------------------------------------------------------------
Model* Phalange::getPhalangeModel(){return model_;}
//---------------------------------------------------------------------
tf::Vector3* Phalange::getContactPositon(){return ct_pos_;}
//---------------------------------------------------------------------
tf::Quaternion* Phalange::getContactOrientation(){return ct_ori_;}
//---------------------------------------------------------------------
tf::Vector3 Phalange::averageVectors(std::vector<geometry_msgs::Vector3> const & vecs)
{
  tf::Vector3 avg_v(0,0,0);

  if (vecs.size()==0)
    return avg_v;        

  for (unsigned int i=0; i < vecs.size();i++ )
      avg_v+=tf::Vector3(vecs[i].x,vecs[i].y,vecs[i].z);
    
  return avg_v/=vecs.size();
}
//---------------------------------------------------------------------
tf::Quaternion Phalange::projectPose(tf::Vector3 const & normal)
{
  tf::Quaternion ori;
  Eigen::Vector3d n_Cref;  
  btMatrix3x3 Cref_R_C;

  tf::VectorTFToEigen(L_T_Cref_.inverse()*normal,n_Cref);
  n_Cref.normalize();

  Eigen::Matrix3d P = (Eigen::Matrix3d()).setIdentity()-n_Cref*n_Cref.transpose();
  
  tf::VectorEigenToTF(P*Eigen::Vector3d(1,0,0),Cref_R_C[0]);
  tf::VectorEigenToTF(P*Eigen::Vector3d(0,1,0),Cref_R_C[1]);
  tf::VectorEigenToTF(n_Cref,Cref_R_C[2]);
    
  tf::Transform Cref_T_C(Cref_R_C.transpose(),tf::Vector3(0,0,0));


  return L_T_Cref_*Cref_T_C.getRotation();
}
//---------------------------------------------------------------------
bool Phalange::touching(){return touching_;}
//---------------------------------------------------------------------
bool Phalange::setPose(icr::SetPose::Request  &req, icr::SetPose::Response &res)
{
  res.success=false;
  lock_.lock();

  L_T_Cref_.setOrigin(tf::Vector3(req.origin.x,req.origin.y,req.origin.z));
  L_T_Cref_.setRotation(tf::createQuaternionFromRPY(req.rpy.x,req.rpy.y,req.rpy.z));

  lock_.unlock();
  res.success=true;
  return res.success;
}
//---------------------------------------------------------------------
