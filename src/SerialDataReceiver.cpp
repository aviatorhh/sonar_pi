#include "SerialDataReceiver.h"

#include "sonar_pi.h"
#ifdef _WIN32
#include <Windows.h>
#include <Winsock2.h>  // before Windows.h, else Winsock 1 conflict
#include <Ws2tcpip.h>  // needed for ip_mreq definition for multicast
#else
#include <arpa/inet.h>
#include <sys/socket.h>
#endif

PLUGIN_BEGIN_NAMESPACE

#define OPENING_SUCCESS 1
#define DEVICE_NOT_FOUND -1
#define ERROR_WHILE_OPENING -2
#define ERROR_WHILE_GETTING_PARAMETERS -3
#define BAUDS_NOT_RECOGNIZED -4
#define ERROR_WRITING_PARAMETERS -5
#define ERROR_WHILE_WRITING_TIMEOUT -6
#define DATABITS_NOT_RECOGNIZED -7
#define STOPBITS_NOT_RECOGNIZED -8
#define PARITY_NOT_RECOGNIZED -9

SerialDataReceiver::SerialDataReceiver(SonarDisplayWindow* app) : wxThread(wxTHREAD_JOINABLE) {
    m_app = app;
    // m_frequency = DEFAULT_FREQUENCY;
    divider = 32;
}

void SerialDataReceiver::Startup() {
    int error_opening = serial.openDevice(m_app->m_pi->m_serial_port, m_app->m_pi->m_serial_baud);
    m_running = error_opening == 1;

    if (m_running) {
        // SendCommand(MODE_OFF);
        // SendCommand(m_frequency);
        // SendCommand(MODE_SONAR);
    } else {
        switch (error_opening) {
            case DEVICE_NOT_FOUND:
                wxLogMessage(wxString::Format("Port %s not found.", m_app->m_pi->m_serial_port));
                break;
            case ERROR_WHILE_OPENING:
                wxLogMessage(wxString::Format("Port %s could not be opened with baud %d.", m_app->m_pi->m_serial_port, m_app->m_pi->m_serial_baud));
                break;
        }
        wxLogMessage(_T("Will not receive any sonar data nor providing depth information to the system. Serialib error code is %d."), error_opening);
    }

    // enable multicast transmission
    if (m_running && m_app->m_pi->m_serial_send_multicast) {
        m_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (m_fd < 0) {
            perror("socket");
            // TODO: better exception handling
        } else {
#ifndef __WXOSX__
            char val = IP_PMTUDISC_DO;
            setsockopt(m_fd, IPPROTO_IP, IP_MTU_DISCOVER, &val, sizeof(val));
#endif
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = inet_addr(const_cast<char*>((const char*)m_app->m_pi->m_ip_address.ToUTF8()));
            addr.sin_port = htons(m_app->m_pi->m_ip_port);

            addrlen = sizeof(addr);
        }
    }
}

SerialDataReceiver::~SerialDataReceiver() {
    wxCriticalSectionLocker enter(m_app->m_pThreadCS);
    m_app->m_data_receiver = nullptr;
    m_app = nullptr;
};

void SerialDataReceiver::SendCommand(uint8_t cmd) {
    serial.writeBytes(&cmd, 1);
}

void SerialDataReceiver::Shutdown() {
    if (serial.isDeviceOpen()) {
        SendCommand(MODE_OFF);

        serial.closeDevice();
    }
    shutdown(m_fd, SHUT_RDWR);
    CLOSE(m_fd);

    m_running = false;
}

// void SerialDataReceiver::SetFrequency(uint8_t frequency) {
//     if (frequency < 100 || frequency > 250) return;
//     m_frequency = frequency;
// }

uint8_t* SerialDataReceiver::GetData() {
    return buf;
}

void SerialDataReceiver::parse_message(uint8_t* msgbuf) {
    
    // TODO: make flexible
    const uint16_t max_depth = MAX_DEPTH;
    const uint16_t num_samples = NUM_SAMPLES;
    const double sample_resolution = max_depth / static_cast<double>(num_samples);

    // depth
    const uint16_t depth_index = msgbuf[0] + msgbuf[1] * 256;
    wxCommandEvent* evt = new wxCommandEvent(NEW_DEPTH_TYPE);

    const double depth = depth_index * sample_resolution;

    evt->SetInt(depth * 100);  // in cm

    wxQueueEvent(m_app, evt);
    if (m_app->m_pi->m_serial_send_multicast) {
        sendto(
            m_fd,
            reinterpret_cast<char*>(msgbuf),
            3,
            0,
            (struct sockaddr*)&addr,
            addrlen);
    }

    wxCommandEvent* evt2 = new wxCommandEvent(NEW_DATA_TYPE);
memcpy(buf, msgbuf, sizeof(buf));
    wxQueueEvent(m_app, evt2);

    if (m_app->m_pi->m_serial_send_multicast) {
        sendto(
            m_fd,
            reinterpret_cast<char*>(msgbuf),
            51,
            0,
            (struct sockaddr*)&addr,
            addrlen);
    }

    /* if(msgbuf[0]== ID_DEPTH) {

        wxCommandEvent *evt = new wxCommandEvent(NEW_DEPTH_TYPE);

        evt->SetInt(msgbuf[1] *100 + msgbuf[2]);
        wxQueueEvent(m_app, evt);

        if(m_app->m_pi->m_serial_send_multicast) {
            sendto(
                m_fd,
                reinterpret_cast<char*>(msgbuf),
                3,
                0,
                (struct sockaddr *) &addr,
                addrlen
            );
        }


    } else if(msgbuf[0]== ID_SONAR) {

        wxCommandEvent *evt = new wxCommandEvent(NEW_DATA_TYPE);

        //memcpy(buf, msgbuf+1, sizeof(buf));

        wxQueueEvent(m_app, evt);

        if(m_app->m_pi->m_serial_send_multicast) {
            sendto(
                m_fd,
                reinterpret_cast<char*>(msgbuf),
                51,
                0,
                (struct sockaddr *) &addr,
                addrlen
            );
        }
    } */
}

wxThread::ExitCode SerialDataReceiver::Entry() {
    // bool is_8bit = true;
    const uint16_t num_samples = NUM_SAMPLES;
    uint8_t w_fac = 1;  // bytes per sample

    while (!TestDestroy() && m_running) {
        const uint16_t avail = serial.available();
        // wxLogMessage(_T("Serial Data Receiver available bytes: %d"), avail);
        if (avail) {
            // get the header byte
            uint8_t header;
            // wxLogMessage(_T("Reading header byte..."));
            int8_t res = serial.readChar((char*)&header, 100);

            if (res <= 0) {
                wxLogMessage(_T("Error while reading serial data for header byte. %d"), res);
                continue;  // timeout
            }

            if ((header & 0xaa) != 0xaa) {
                continue;
            }

            if (header == 0xaa) {
                // is_8bit = false;
                w_fac = 2;
            } else if (header == 0xab) {
                // is_8bit = true;
                w_fac = 1;
            } else {
                // invalid header byte, skip
                wxLogMessage(_T("Received invalid header byte: 0x%02x"), header);
                continue;
            }
            uint8_t buf[HEADER_SIZE + NUM_SAMPLES * w_fac];  // max message size

            uint16_t expected_msg_length = HEADER_SIZE + num_samples * w_fac;  // header + payload + checksum

            // buf = (uint8_t*)malloc(expected_msg_length * sizeof(uint8_t));

            // wxLogMessage(_T("Reading message of %d bytes..."), expected_msg_length);

            uint16_t readBytes = serial.readBytes(buf, expected_msg_length, 100);
            uint8_t checksum;
            // wxLogMessage(_T("Reading checksum byte..."));
            res = serial.readBytes((char*)&checksum, 100);
            if (res <= 0) {
                wxLogMessage(_T("Error while reading serial data for checksum byte. %d"), res);
                continue;  // timeout
            }

            if (readBytes < expected_msg_length) {
                wxLogMessage(_T("Timeout while reading serial data. Expected %d bytes, received %d bytes."), expected_msg_length, readBytes);
                // free(_buf);
                continue;  // timeout
            }

            // wxLogMessage(_T("Received message of %d bytes, received %d bytes."), expected_msg_length + 1, readBytes);
            // calculate checksum
            uint8_t csum = 0;
            for (uint16_t i = 0; i < expected_msg_length; i++) {
                csum ^= buf[i];
            }

            if (csum != checksum) {
                wxLogMessage(_T("Received invalid checksum. Calculated: 0x%02x, Received: 0x%02x"), csum, checksum);
                // invalid checksum, skip message
                // free(_buf);
                continue;
            }
            // wxLogMessage(_T("Received checksum ok. Calculated: 0x%02x, Received: 0x%02x"), csum, checksum);

            // buf = (uint8_t*)malloc(expected_msg_length * sizeof(uint8_t));
            // memcpy(buf, _buf, expected_msg_length);
            // free(_buf);

           // if (divider-- == 0) {
           //m_frequency     divider = 32;
                parse_message(buf);
         //   }

            // parse_message(buf);

            // if (buf[0] == ID_DEPTH) {
            //     serial.readBytes(buf + 1, 2);
            //     parse_message(buf);
            // } else if (buf[0] == ID_SONAR) {
            //     serial.readBytes(buf + 1, 50);
            //     parse_message(buf);
            // }
        }
    }
#ifdef _WIN32
    // WSACleanup();
#endif

    wxLogMessage(_T("Serial Data Receiver Receiption Thread ended."));

    return (wxThread::ExitCode)0;  // success;
}

PLUGIN_END_NAMESPACE
