#pragma once

#include <chrono>
#include <iostream>
#include <string>

#include "Types.h"

namespace tomcat {
    namespace model {

        // value filled in the data files for time steps where there's no
        // observation for a given node.
#define NO_OBS -1
#define LOG(log) std::cout << log << std::endl
#define LOG_WARNING(log) std::cerr << log << std::endl
#define EXISTS(member, container) (container.find(member) != container.end())
#define EPSILON 1E-15
#define EMPTY_VECTOR Eigen::VectorXd(0)

        /**
         * General exception raised throughout the project
         */
        struct TomcatModelException : public std::exception {
            std::string message;

            TomcatModelException(const std::string& message)
                : message(message) {}

            const char* what() const throw() { return this->message.data(); }
        };

        /**
         * Auxiliary struct to measure the execution time within a block.
         */
        struct Timer {

            typedef std::chrono::seconds seconds;

            std::chrono::time_point<std::chrono::steady_clock> start, end;
            std::chrono::duration<float> duration;

            Timer() { this->start = std::chrono::steady_clock::now(); }

            ~Timer() {
                this->end = std::chrono::steady_clock::now();
                this->duration = this->end - this->start;

                std::cout << "Timer took " << this->duration.count()
                          << "seconds.\n";
            }
        };

    } // namespace model
} // namespace tomcat
