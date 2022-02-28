#ifndef __OBJECTS_H__
#define __OBJECTS_H__
#include "Headers/Common.hpp"
#include "Headers/Utils.hpp"
#include "Headers/Objects.hpp"
#include "Headers/Publishers.hpp"
#include "Headers/PointClouds.hpp"
#include "Headers/Accumulator.hpp"
#include "Headers/Compensator.hpp"
#include "Headers/Localizator.hpp"
#include "Headers/Mapper.hpp"
#endif

extern struct Params Config;

// class State {
    // public:
        State::State() {
            // Read from YAML
            this->g = Eigen::Map<Eigen::Vector3f>(Config.initial_gravity.data(), 3);
            this->tLI = Eigen::Map<Eigen::Vector3f>(Config.I_Translation_L.data(), 3);
            this->RLI = Eigen::Map<Eigen::Matrix3f>(Config.I_Rotation_L.data(), 3, 3).transpose();

            // Assume object is static at the begging
            this->vel = Eigen::Vector3f::Zero();

            // Assume biases and noises zero as a first estimation
            this->bw = Eigen::Vector3f::Zero();
            this->ba = Eigen::Vector3f::Zero();

            this->nw = Eigen::Vector3f::Zero();
            this->na = Eigen::Vector3f::Zero();
            this->nbw = Eigen::Vector3f::Zero();
            this->nba = Eigen::Vector3f::Zero();

            // Assume vehicle at origin
            this->pos = Eigen::Vector3f::Zero();
            this->R = Eigen::Matrix3f::Identity();
        }

        State::State(double time) : State::State() {
            this->time = time;
        }

        State::State(const state_ikfom& s, double time) : State () {
            // Export state_ikfom parameters
            this->R = s.rot.toRotationMatrix().cast<float>();
            this->pos = s.pos.cast<float>();
            this->vel = s.vel.cast<float>();
            
            this->bw = s.bg.cast<float>();
            this->ba = s.ba.cast<float>();

            this->RLI = s.offset_R_L_I.toRotationMatrix().cast<float>();
            this->tLI = s.offset_T_L_I.cast<float>();
            
            // Get closest IMU to time
            IMU imu = Accumulator::getInstance().get_next_imu(time);
            this->a = imu.a;
            this->w = imu.w;

            this->time = time;
        }

        RotTransl State::I_Rt_L() const {
            return RotTransl(
                this->RLI,
                this->tLI
            );
        }

        RotTransl State::inv() const {
            return RotTransl(*this).inv();
        }

        void State::operator+= (const IMU& imu) {
            this->update(imu);
        }

        Point operator* (const State& X, const Point& p) {
            return RotTransl(X) * p;
        }

        RotTransl operator* (const State& X, const RotTransl& RT) {
            return RotTransl(X) * RT;
        }

        Points operator* (const State& X, const Points& points) {
            return RotTransl(X) * points;
        }

    // private:
    
        // When propagating, we set noises = 0
        void State::propagate_f(IMU imu, float dt) {
            Eigen::Matrix3f Rtemp = this->R;
            Eigen::Vector3f veltemp = this->vel;
            Eigen::Vector3f postemp = this->pos;

            // R ⊞ (w - bw - nw)*dt
            // v ⊞ (R*(a - ba - na) + g)*dt
            // p ⊞ (v*dt + 1/2*(R*(a - ba - na) + g)*dt*dt)

            Rtemp *= SO3Math::Exp(Eigen::Vector3f(imu.w - this->bw), dt);
            veltemp += (this->R*(imu.a - this->ba) - this->g)*dt;
            postemp += this->vel*dt + 0.5*(this->R*(imu.a - this->ba) - this->g)*dt*dt;

            this->R = Rtemp;
            this->vel = veltemp;
            this->pos = postemp;
        }

        void State::update(IMU imu) {
            // Xt = Xt_1 ⊞ f(imu)*dt
            float dt = imu.time - time;
            this->propagate_f(imu, dt);

            // Update last controls
            this->time = imu.time;
            this->a = 0.5*this->a + 0.5*imu.a;  // Exponential mean (noisy inputs)
            this->w = 0.5*this->w + 0.5*imu.w;  // Exponential mean (noisy inputs)
        }  