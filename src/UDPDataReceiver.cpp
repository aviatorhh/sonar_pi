

#include "UDPDataReceiver.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <iostream>

#include "SonarDisplayWindow.h"
#include "sonar_pi.h"

PLUGIN_BEGIN_NAMESPACE

UDPDataReceiver::UDPDataReceiver(SonarDisplayWindow* app, sonar_pi* sp) : wxThread(wxTHREAD_JOINABLE) {
    m_app = app;
    m_pi = sp;
    m_has_addr = false;
}

UDPDataReceiver::~UDPDataReceiver() {
    wxCriticalSectionLocker enter(m_app->m_pThreadCS);
    m_app->m_data_receiver = nullptr;
    m_app = nullptr;
};

void UDPDataReceiver::Startup() {
    // TODO: implement only if a secure way can be found
    /*SendCommand(MODE_OFF);
    SendCommand(m_frequency);
    SendCommand(MODE_SONAR)*/
    ;

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
    //
    // Initialize Windows Socket API with given VERSION.
    //
    WSADATA wsaData;
    if (WSAStartup(0x0101, &wsaData)) {
        perror("WSAStartup");
        // return (wxThread::ExitCode)1;
    }
#endif

    // create what looks like an ordinary UDP socket
    //
    fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (fd < 0) {
        perror("socket");
        // TODO: erro handling
    }

    // allow multiple sockets to use the same PORT number
    //
    u_int yes = 1;
    if (
        setsockopt(
            fd, SOL_SOCKET, SO_REUSEADDR, (char*)&yes, sizeof(yes)) < 0) {
        perror("Reusing ADDR failed");
        // TODO: erro handling
    }

    // set up destination address
    //
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);  // differs from sender
    addr.sin_port = htons(m_pi->m_ip_port);

    // bind to receive address
    //
    if (::bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        // TODO: erro handling
    }

    // use setsockopt() to request that the kernel join a multicast group
    //
    struct ip_mreq mreq;
    mreq.imr_multiaddr.s_addr = inet_addr(const_cast<char*>((const char*)m_pi->m_ip_address.ToUTF8()));
    mreq.imr_interface.s_addr = htonl(INADDR_ANY);
    if (
        setsockopt(
            fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) < 0) {
        perror("setsockopt");
        // TODO: error handling
    }
}

// void UDPDataReceiver::SetFrequency(wxUint8 frequency) {
//     if (frequency < 100 || frequency > 250) return;
//     m_frequency = frequency;
// }

uint8_t* UDPDataReceiver::GetData() {
    return buf;
}

void UDPDataReceiver::parse_message(uint8_t* msgbuf) {
    // TODO: make flexible
    const uint16_t max_depth = MAX_DEPTH;
    const uint16_t num_samples = NUM_SAMPLES;
    const double sample_resolution = max_depth / static_cast<double>(num_samples);

    // depth
    const uint16_t depth_index = msgbuf[1] + msgbuf[2] * 256;
    wxCommandEvent* evt = new wxCommandEvent(NEW_DEPTH_TYPE);

    const double depth = depth_index * sample_resolution;

    evt->SetInt(depth * 100);  // in cm

    wxQueueEvent(m_app, evt);

    wxCommandEvent* evt2 = new wxCommandEvent(NEW_DATA_TYPE);
    memcpy(buf, msgbuf+1, sizeof(buf));
    wxQueueEvent(m_app, evt2);
}
void UDPDataReceiver::SendCommand(char cmd) {
    if (!m_has_addr) return;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr = m_remote_ip_address;
    addr.sin_port = htons(m_pi->m_ip_port + 1);

    int _fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr* sa = (struct sockaddr*)&addr;
    int addrlen = sizeof(addr);

    int status = connect(_fd, sa, addrlen);
    if (status == 0) {
        char _b[1] = {cmd};

        send(_fd, _b, 1, 0);

        shutdown(_fd, SHUT_WR);
    }
    CLOSE(_fd);
}

wxThread::ExitCode UDPDataReceiver::Entry() {
    const uint16_t num_samples = NUM_SAMPLES;

    uint8_t w_fac = 1;                                      // bytes per sample
    uint8_t msgbuf[HEADER_SIZE + 2 + NUM_SAMPLES * w_fac];  // max message size

    struct sockaddr_in from;
    socklen_t len = sizeof(from);

    while (!TestDestroy()) {
        int nbytes = recvfrom(
            fd,
            reinterpret_cast<char*>(msgbuf),
            HEADER_SIZE + 2 + NUM_SAMPLES * w_fac,
            0,
            (struct sockaddr*)&from,
            &len);
        if (nbytes == -1) {
            continue;
        }
        if (nbytes < 0) {
            perror("recvfrom");
            return (wxThread::ExitCode)0;
        }

        if (nbytes > 0 && msgbuf[0] == 0xab) {
            uint8_t checksum = msgbuf[HEADER_SIZE + 2 + NUM_SAMPLES * w_fac - 1];

            uint16_t expected_msg_length = HEADER_SIZE + num_samples * w_fac;
            // wxLogMessage(_T("Received message of %d bytes, received %d bytes."), expected_msg_length + 1, readBytes);
            // calculate checksum
            uint8_t csum = 0;
            for (uint16_t i = 0; i < expected_msg_length; i++) {
                csum ^= msgbuf[i + 1];
            }

            if (csum != checksum) {
                wxLogMessage(_T("Received invalid checksum. Calculated: 0x%02x, Received: 0x%02x"), csum, checksum);
                // invalid checksum, skip message
                // free(_buf);
                continue;
            }

            parse_message(msgbuf);
        }
    }

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
    WSACleanup();
#endif

    return (wxThread::ExitCode)0;  // success;
}

void UDPDataReceiver::Shutdown() {
    shutdown(fd, SHUT_RDWR);
    CLOSE(fd);
}

PLUGIN_END_NAMESPACE
