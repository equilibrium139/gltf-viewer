#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vector>

#undef near
#undef far
#undef NEAR
#undef FAR

enum Camera_Movement
{
    CAM_FORWARD,
    CAM_BACKWARD,
    CAM_LEFT,
    CAM_RIGHT
};

const float YAW = -90.0f;
const float PITCH = 0.0f;
const float SPEED = 1.5f;
const float SENSITIVITY = 0.1f;
const float ZOOM = 45.0f;
const float NEAR = 0.001f;
const float FAR = 1000.0f;


class Camera
{
public:
    std::string name;
    // camera Attributes
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;
    // euler Angles
    float yaw;
    float pitch;
    // camera options
    float movementSpeed;
    float mouseSensitivity;
    float zoom;
    float near;
    float far;

    // constructor with vectors
    Camera(glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = YAW, float pitch = PITCH, float near = NEAR, float far = FAR)
        : position(position), front(glm::vec3(0.0f, 0.0f, -1.0f)), worldUp(up), yaw(yaw), pitch(pitch), movementSpeed(SPEED), mouseSensitivity(SENSITIVITY), zoom(ZOOM),
        near(near), far(far)
    {
        updateCameraVectors();
    }

    // constructor with scalar values
    Camera(float posX, float posY, float posZ, float upX, float upY, float upZ, float yaw = YAW, float pitch = PITCH, float near = NEAR, float far = FAR)
        : position(glm::vec3(posX, posY, posZ)), front(glm::vec3(0.0f, 0.0f, -1.0f)), worldUp(glm::vec3(upX, upY, upZ)), movementSpeed(SPEED), mouseSensitivity(SENSITIVITY),
        zoom(ZOOM), yaw(yaw), pitch(pitch), near(near), far(far)
    {
        updateCameraVectors();
    }

    // returns the view matrix calculated using Euler Angles and the LookAt Matrix
    glm::mat4 GetViewMatrix() const
    {
        return glm::lookAt(position, position + front, up);
    }

    glm::mat4 GetProjectionMatrix(float aspect) const
    {
        return glm::perspective(glm::radians(zoom), aspect, near, far);
    }

    // processes input received from any keyboard-like input system. Accepts input parameter in the form of camera defined ENUM (to abstract it from windowing systems)
    void ProcessKeyboard(Camera_Movement direction, float deltaTime)
    {
        float velocity = movementSpeed * deltaTime;
        if (direction == CAM_FORWARD)
            position += front * velocity;
        if (direction == CAM_BACKWARD)
            position -= front * velocity;
        if (direction == CAM_LEFT)
            position -= right * velocity;
        if (direction == CAM_RIGHT)
            position += right * velocity;
    }

    // processes input received from a mouse input system. Expects the offset value in both the x and y direction.
    void ProcessMouseMovement(float xoffset, float yoffset, bool constrainPitch = true)
    {
        xoffset *= mouseSensitivity;
        yoffset *= mouseSensitivity;

        yaw += xoffset;
        pitch += yoffset;

        // make sure that when pitch is out of bounds, screen doesn't get flipped
        if (constrainPitch)
        {
            if (pitch > 89.0f)
                pitch = 89.0f;
            if (pitch < -89.0f)
                pitch = -89.0f;
        }

        // update Front, Right and Up Vectors using the updated Euler angles
        updateCameraVectors();
    }

    // processes input received from a mouse scroll-wheel event. Only requires input on the vertical wheel-axis
    void ProcessMouseScroll(float yoffset)
    {
        zoom -= (float)yoffset;
        if (zoom < 1.0f)
            zoom = 1.0f;
        if (zoom > 45.0f)
            zoom = 45.0f;
    }

    void LookAt(glm::vec3 pos)
    {
        front = glm::normalize(pos - this->position);
        pitch = glm::degrees(asin(front.y));
        yaw = glm::degrees(atan2(front.z, front.x));
        updateCameraVectors();
        // TODO: Investigate
        /*right = glm::normalize(glm::cross(front, worldUp));
        up = glm::normalize(glm::cross(right, front));*/
    }

private:
    // calculates the front vector from the Camera's (updated) Euler Angles
    void updateCameraVectors()
    {
        // calculate the new Front vector
        front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        front.y = sin(glm::radians(pitch));
        front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        front = glm::normalize(front);
        // also re-calculate the Right and Up vector
        right = glm::normalize(glm::cross(front, worldUp));  // normalize the vectors, because their length gets closer to 0 the more you look up or down which results in slower movement.
        up = glm::normalize(glm::cross(right, front));
    }
};