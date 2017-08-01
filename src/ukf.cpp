#include "ukf.h"
#include "Eigen/Dense"
#include <iostream>

using namespace std;
using Eigen::MatrixXd;
using Eigen::VectorXd;
using std::vector;

/**
 * Initializes Unscented Kalman filter
 */
UKF::UKF() {
  // if this is false, laser measurements will be ignored (except during init)
  use_laser_ = true;

  // if this is false, radar measurements will be ignored (except during init)
  use_radar_ = true;

  // initial state vector
  x_ = VectorXd(5);

  // initial covariance matrix
  P_ = MatrixXd(5, 5);

  // Process noise standard deviation longitudinal acceleration in m/s^2
  std_a_ = 2.0;

  // Process noise standard deviation yaw acceleration in rad/s^2
  std_yawdd_ = 0.4;

  // Laser measurement noise standard deviation position1 in m
  std_laspx_ = 0.15;

  // Laser measurement noise standard deviation position2 in m
  std_laspy_ = 0.15;

  // Radar measurement noise standard deviation radius in m
  std_radr_ = 0.3;

  // Radar measurement noise standard deviation angle in rad
  std_radphi_ = 0.03;

  // Radar measurement noise standard deviation radius change in m/s
  std_radrd_ = 0.3;

  /**
  DONE:

  Complete the initialization. See ukf.h for other member properties.

  Hint: one or more values initialized above might be wildly off...
    --std_a_ & std_yawdd_ were set to 30, yes wildly
    --these params tuned manually for results 
  */
  n_x_ = 5;
  n_aug_ = 7;
  n_sig_ = 2*n_aug_ + 1;
  lambda_ = 3 - n_aug_;
  P_ << 0.15,    0,    0,    0,    0, 
           0, 0.15,    0,    0,    0, 
           0,    0,    1,    0,    0, 
           0,    0,    0,    1,    0,
           0,    0,    0,    0,    1;

  Xsig_pred_ = MatrixXd(n_x_, n_sig_);
 
  weights_ = VectorXd(n_sig_);

  is_initialized_ = false;
  
}

UKF::~UKF() {}

/**
 * @param {MeasurementPackage} meas_package The latest measurement data of
 * either radar or laser.
 */
void UKF::ProcessMeasurement(MeasurementPackage meas_package) {
  /**
  DONE:

  Complete this function! Make sure you switch between lidar and radar
  measurements.
  */
  // if both sensors active 
  if ((meas_package.sensor_type_ == MeasurementPackage:: RADAR && use_radar_) ||
     (meas_package.sensor_type_ == MeasurementPackage:: LASER && use_laser_))
  {
    if (!is_initialized_)
    {
        std::cout << "UKF: " << std::endl;
    
      if (meas_package.sensor_type_ == MeasurementPackage:: LASER && use_laser_)
      {
        // initialize state
        x_(0) = meas_package.raw_measurements_(0);
        x_(1) = meas_package.raw_measurements_(1);
      }
      else if (meas_package.sensor_type_ == MeasurementPackage:: RADAR && use_radar_)  
      {
        // greeks for polar conversion
        float rho = meas_package.raw_measurements_(0);
        float theta = meas_package.raw_measurements_(1);

        // initialize state
        x_(0) = rho * cos(theta);
        x_(1) = rho * sin(theta);
      }

      time_us_ = meas_package.timestamp_;

      // initialization complete
      is_initialized_ = true; 
      
      return;

    }//if initialized
    
    /***** Prediction ******/
    
    // calc delta t (units of seconds)
    float dt = (meas_package.timestamp_ - time_us_) / 1000000.0;
    time_us_ = meas_package.timestamp_;
    Prediction(dt);

    /***** Update *****/
    
    if (meas_package.sensor_type_ == MeasurementPackage:: LASER && use_laser_)
      UpdateLidar(meas_package);
    else if (meas_package.sensor_type_ == MeasurementPackage:: RADAR && use_radar_) 
      UpdateRadar(meas_package); 
      
  } // if both sensors active
  
} //ProcessMeasurement

/**
 * Predicts sigma points, the state, and the state covariance matrix.
 * @param {double} delta_t the change in time (in seconds) between the last
 * measurement and this one.
 */
void UKF::Prediction(double delta_t) {
  /**
  DONE:

  Complete this function! Estimate the object's location. Modify the state
  vector, x_. Predict sigma points, the state, and the state covariance matrix.
  */
  
  // create augmented mean vector
  VectorXd x_aug = VectorXd(n_aug_);

  // create augmented mean state
  x_aug.head(5) = x_;
  x_aug(5) = 0;
  x_aug(6) = 0;

  // create augmented state covariance Matrix
  MatrixXd P_aug = MatrixXd(n_aug_, n_aug_);
  P_aug.fill(0.0);
  P_aug.topLeftCorner(5,5) = P_;
  P_aug(5,5) = std_a_ * std_a_;
  P_aug(6,6) = std_yawdd_ * std_yawdd_;

  // create sq rt Matrix
  MatrixXd L = P_aug.llt().matrixL();

  // create sigma point Matrix
  MatrixXd Xsig_aug = MatrixXd(n_aug_,n_sig_);

  // create augmented sigma points
  Xsig_aug.col(0) = x_aug;
  for (int i = 0; i<n_aug_; i++)
  {
    Xsig_aug.col(i+1) = x_aug + std::sqrt(lambda_ + n_aug_) * L.col(i);
    Xsig_aug.col(i+1+n_aug_) = x_aug - std::sqrt(lambda_ + n_aug_) * L.col(i);
  }

  /***** predict sigma points *****/
  for (int i = 0; i < n_sig_; i++)
  {
    // pull values from sigma point Matrix for readability
    double p_x = Xsig_aug(0,i);
    double p_y = Xsig_aug(1,i);
    double v = Xsig_aug(2,i);
    double yaw = Xsig_aug(3,i);
    double yawd = Xsig_aug(4,i);
    double nu_a = Xsig_aug(5, i);
    double nu_yawdd = Xsig_aug(6,i);

    // declare predicted values
    double px_p;
    double py_p;
    double v_p = v;
    double yaw_p = yaw + yawd*delta_t;
    double yawd_p = yawd;

    if (fabs(yawd) > 0.001)
    {
      px_p = p_x + v/yawd * (sin(yaw + yawd*delta_t));
      py_p = p_y + v/yawd * (cos(yaw) - cos(yaw+yawd*delta_t));
    }
    else
    {
      px_p = p_x + v*delta_t*cos(yaw);
      py_p = p_y + v*delta_t*sin(yaw);
    }

    // predicted values + noise
    px_p += 0.5*nu_a*delta_t*delta_t*cos(yaw);
    py_p += 0.5*nu_a*delta_t*delta_t*sin(yaw);
    v_p += nu_a*delta_t;
    yaw_p += 0.5*nu_yawdd*delta_t*delta_t;
    yawd_p += nu_yawdd*delta_t;

    // fill Xsig_pred with column vectors
    Xsig_pred_(0,i) = px_p; 
    Xsig_pred_(1,i) = py_p;
    Xsig_pred_(2,i) = v_p;
    Xsig_pred_(3,i) = yaw_p;
    Xsig_pred_(4,i) = yawd_p; 
  
  }

  /***** predict mean and covariance *****/
  
  //set weights
  double weight_0 = lambda_/(lambda_ + n_aug_);
  weights_(0) = weight_0;
  for (int i=1; i < n_sig_ + 1; i++)
  {
    double weight = 0.5/(n_aug_ + lambda_);
    weights_(i) = weight;
  }

  // predict state mean
  // note iteration over sigma points
  /*x_.fill(0.0);
  for (int i=0; i < n_sig_; i++)
  {
    x_ = x_ + weights_(i) * Xsig_pred_.col(i);
  }*/
  x_ = Xsig_pred_ * weights_;

  // predict state covariance Matrix P_
  // note iteration over sigma points
  P_.fill(0.0);
  for (int i = 0; i < n_sig_; i++)
  {
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    
    // angle normalization
    while (x_diff(3) > M_PI) x_diff(3)-=2.*M_PI; 
    while (x_diff(3) < -M_PI) x_diff(3)+=2.*M_PI; 

    P_ += weights_(i) * x_diff * x_diff.transpose();
  }
} //Prediction

/**
 * Updates the state and the state covariance matrix using a laser measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateLidar(MeasurementPackage meas_package) {
  /**
  DONE:

  Complete this function! Use lidar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the lidar NIS.
  */
  /***** predict Lidar sigma points *****/

  // set meas dimension
  int n_z = 2;

  // create Matrix for sigma points in meas space
  MatrixXd Zsig = Xsig_pred_.block(0,0,n_z,n_sig_);
  /* 
  MatrixXd Zsig = MatrixXd(n_z, n_sig_);

  // transform sigma points into measurement space
  for (int i = 0; i < n_sig_; i++)
  {
    //pull values for readibility 
    double p_x = Xsig_pred_(0,i);
    double p_y = Xsig_pred_(1,i);

    // measurement model px & py
    Zsig(0,i) = p_x;
    Zsig(1,i) = p_y;
  }*/

  // predicted measurement
  VectorXd z_pred = VectorXd(n_z);
  z_pred.fill(0.0);
  for (int i=0; i < n_sig_; i++) 
  {
      z_pred = z_pred + weights_(i) * Zsig.col(i);
  }


  // create covariance Matrix S
  MatrixXd S = MatrixXd(n_z,n_z);
  S.fill(0.0);
  for (int i = 0; i < n_sig_; i++)
  {
    VectorXd z_diff = Zsig.col(i) - z_pred;
    S += weights_(i) * z_diff * z_diff.transpose();
  }

  // measurement covariance Matrix add noise
  MatrixXd R = MatrixXd(n_z,n_z);
  R << std_laspx_*std_laspx_,0,0,std_laspy_*std_laspy_;
  S += R;

  /****** Measurement Update *****/

  // create cross correlation Matrix Tc
  MatrixXd Tc = MatrixXd(n_x_,n_z);

  Tc.fill(0.0);
  for (int i = 0; i < n_sig_; i++)
  {
    // calc residuals
    VectorXd z_diff = Zsig.col(i) - z_diff;

    // calc state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;

    Tc += weights_(i) * z_diff.transpose() * x_diff;
  }

  // calc Kalman gain K
  MatrixXd K = Tc * S.inverse();

  float p_x = meas_package.raw_measurements_(0);
  float p_y = meas_package.raw_measurements_(1);

  // fill vector with meas values
  VectorXd z = VectorXd(n_z);
  z << p_x, p_y;

  // calc residuals
  VectorXd z_diff = z - z_pred;

  // update state and covariance Matrix
  x_ += K * z_diff;
  P_ -= K*S*K.transpose();

  // NIS
  double NIS_lidar = z_diff.transpose() * S.inverse() * z_diff;

}

/**
 * Updates the state and the state covariance matrix using a radar measurement.
 * @param {MeasurementPackage} meas_package
 */
void UKF::UpdateRadar(MeasurementPackage meas_package) {
  /**
  DONE:

  Complete this function! Use radar data to update the belief about the object's
  position. Modify the state vector, x_, and covariance, P_.

  You'll also need to calculate the radar NIS.
  */
  
  //set meas dimenstion - polar, rho, theta, rho_dot
  int n_z = 3;
  
  // create Matrix for sigma point in meas space
  MatrixXd Zsig = MatrixXd(n_z, n_sig_);
  
  // transform sigma points into measurement space
  for (int i = 0; i < n_sig_; i++)
  {
    //pull values for readibility 
    double p_x = Xsig_pred_(0,i);
    double p_y = Xsig_pred_(1,i);
    double v = Xsig_pred_(2,i);
    double yaw = Xsig_pred_(3,i);
    double v_x = v * cos(yaw);
    double v_y = v * sin(yaw);

    // measurement model px & py
    Zsig(0,i) = sqrt(p_x*p_x + p_y*p_y); //rho
    Zsig(1,i) = atan2(p_y,p_x); //theta
    Zsig(2,i) = (p_x*v_x + p_y*v_y) / sqrt(p_x*p_x + p_y*p_y); //rho_dot
  }

  // predicted measurement
  VectorXd z_pred = VectorXd(n_z);
  z_pred.fill(0.0);
  for (int i=0; i < n_sig_; i++)
  { 
    z_pred += weights_(i) * Zsig.col(i);
  }
  
  // measurement covariance Matrix S
  MatrixXd S = MatrixXd(n_z,n_z);
  S.fill(0.0);
  for (int i=0; i < n_sig_; i++)
  {
    // calc residuals
    VectorXd z_diff = Zsig.col(i) - z_pred;

    // angle normalization
    while (z_diff(1) > M_PI) z_diff(1)-=2.*M_PI; 
    while (z_diff(1) < -M_PI) z_diff(1)+=2.*M_PI; 
 
    S += weights_(i) * z_diff * z_diff.transpose();
  }

  // add meas noise covariance Matrix
  MatrixXd R  = MatrixXd(n_z,n_z);
  R << std_radr_*std_radr_,0,0,0,
       std_radphi_*std_radphi_,0,0,0,
       std_radrd_*std_radrd_;

  S += R;

  /****** Measurement Update *****/

  // cross correlation Matrix Tc
  MatrixXd Tc = MatrixXd(n_z,n_z);
  
  Tc.fill(0.0);
  for (int i=0; i < n_sig_; i++) 
  {  
    // calc residual
    VectorXd z_diff = Zsig.col(i) - z_pred;
    // angle normalization
    while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
    while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

    // state difference
    VectorXd x_diff = Xsig_pred_.col(i) - x_;
    // angle normalization
    while (x_diff(3)> M_PI) x_diff(3)-=2.*M_PI;
    while (x_diff(3)<-M_PI) x_diff(3)+=2.*M_PI;

    Tc = Tc + weights_(i) * x_diff * z_diff.transpose();
  }

  // Kalman gain K;
  MatrixXd K = Tc * S.inverse();

  float ro = meas_package.raw_measurements_(0);
  float theta = meas_package.raw_measurements_(1);
  float ro_dot = meas_package.raw_measurements_(2);

  VectorXd z = VectorXd(n_z);
  z <<
    ro,
    theta,
    ro_dot;

  // residual
  VectorXd z_diff = z - z_pred;

  //angle normalization
  while (z_diff(1)> M_PI) z_diff(1)-=2.*M_PI;
  while (z_diff(1)<-M_PI) z_diff(1)+=2.*M_PI;

  //NIS
  double NIS_radar = z_diff.transpose() * S.inverse() * z_diff;

  //update state mean and covariance matrix
  x_ = x_ + K * z_diff;
  P_ = P_ - K*S*K.transpose();    

}
