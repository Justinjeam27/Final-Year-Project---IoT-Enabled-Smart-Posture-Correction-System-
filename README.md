Project Description: IoT-Enabled Smart Posture Correction System with Real-Time Activity Monitoring and Analysis
Overview
This project is an advanced, wearable health-technology device designed to combat Forward Head Posture (FHP), commonly known as "Text Neck." Unlike traditional, bulky mechanical back braces that restrict movement, this system utilizes a discreet, ergonomic silicone neckband equipped with dual kinematic sensors to provide real-time postural tracking, active correction, and remote telehealth monitoring.

Hardware Architecture
At the core of the physical build is the highly efficient XIAO ESP32-S3 microcontroller. It continuously processes spatial data gathered via I2C communication from two MPU6050 Inertial Measurement Units (IMUs)—one calibrated for the cervical spine (neck) and one for the upper back. The system applies a complementary filter algorithm to stabilize the raw accelerometer noise and gyroscope drift, accurately calculating the user's exact cervical angle in real-time.

Active Feedback & IoT Integration
The system operates on a seamless, closed-loop architecture:

Immediate Local Correction: When the microcontroller detects a forward tilt that exceeds the healthy clinical threshold (e.g., >15 degrees for a sustained period), it triggers an onboard active buzzer. This provides immediate localized feedback, prompting the user to subconsciously correct their posture.

Remote Telehealth Monitoring: Simultaneously, the ESP32-S3 leverages its integrated Wi-Fi capabilities to securely transmit the filtered angular data to a public IoT cloud dashboard (Blynk). This integration translates raw telemetry into a highly customizable Graphical User Interface (GUI), allowing users and healthcare professionals to visualize historical posture data, track long-term habits, and monitor corrective progress through readable gauges and graphs.

Key Engineering Innovations
Ergonomic Wearability: Transitions the industry standard from heavy mid-spine harnesses to a lightweight, flexible silicone cervical band.

High-Fidelity Sensor Fusion: Utilizes custom C++ filtering logic to ensure physical movement (walking/typing) does not trigger false-positive posture alerts.

Power Efficiency: Engineered to maintain long battery life during continuous Wi-Fi transmission, ensuring the device can be worn throughout a standard workday.
