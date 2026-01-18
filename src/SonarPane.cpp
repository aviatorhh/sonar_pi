#include "SonarPane.h"

#include <wx/glcanvas.h>

#include <algorithm>
#include <cstdint>

#include "pi_common.h"
#include "sonar_pi.h"

PLUGIN_BEGIN_NAMESPACE

static int attribs[] = {WX_GL_RGBA, WX_GL_DOUBLEBUFFER, WX_GL_DEPTH_SIZE, 0, WX_GL_SAMPLE_BUFFERS, 1, 0};

SonarPane::SonarPane(SonarDisplayWindow* parent)
    : wxGLCanvas(parent, wxID_ANY, attribs, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE, _T("")) {
    _x = this->GetSize().GetWidth();
    _y = this->GetSize().GetHeight();
    m_parent = parent;

    initColorMap();

    if (m_parent->m_pi->IsOpenGLEnabled()) {
        m_context = new wxGLContext(this);
        Bind(wxEVT_PAINT, &SonarPane::Render, this);
        Bind(wxEVT_SIZE, &SonarPane::OnResize, this);
    } else {
        m_context = nullptr;
        Hide();
    }
}

void SonarPane::ResetDataBuffer() {
    memset(data_buffer, 0, BUF_X * NUM_SAMPLES * sizeof(uint8_t));
}

SonarPane::~SonarPane() {
    if (m_context) {
        Unbind(wxEVT_PAINT, &SonarPane::Render, this);
        Unbind(wxEVT_SIZE, &SonarPane::OnResize, this);
        delete m_context;
    }
}
/**
 * @brief Initialize the color map based on the current display mode
 */
void SonarPane::initColorMap() {
    if (m_parent->m_display_mode == MONOCHROME) {
        buildSonarMonochrommap(CYCLIC_GRADIENT);
    } else if (m_parent->m_display_mode == MULTICOLOR) {
        buildSonarColormap(CYCLIC_GRADIENT);
    }

    for (int i = 0; i < 256; ++i) {
        const uint8_t r = CYCLIC_GRADIENT[i][0];
        const uint8_t g = CYCLIC_GRADIENT[i][1];
        const uint8_t b = CYCLIC_GRADIENT[i][2];
        colorMap[i] = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    }
}
/**
 * @brief Build a cyclic colormap similar to the 'cyclic' colormap in PyQtGraph
 */
void SonarPane::buildSonarColormap(uint8_t colormap[256][3]) {
    for (int i = 0; i < 256; ++i) {
        float t = i / 255.0f;

        float r, g, b;

        if (t < 0.25f) {
            // Black → Blue
            r = 0.0f;
            g = 0.0f;
            b = t / 0.25f;
        } else if (t < 0.50f) {
            // Blue → Cyan
            r = 0.0f;
            g = (t - 0.25f) / 0.25f;
            b = 1.0f;
        } else if (t < 0.75f) {
            // Cyan → Yellow
            r = (t - 0.50f) / 0.25f;
            g = 1.0f;
            b = 1.0f - (t - 0.50f) / 0.25f;
        } else {
            // Yellow → Red → White
            r = 1.0f;
            g = 1.0f - (t - 0.75f) / 0.25f;
            b = (t - 0.75f) / 0.25f;
        }

        colormap[i][0] = static_cast<uint8_t>(std::clamp(r, 0.0f, 1.0f) * 255);
        colormap[i][1] = static_cast<uint8_t>(std::clamp(g, 0.0f, 1.0f) * 255);
        colormap[i][2] = static_cast<uint8_t>(std::clamp(b, 0.0f, 1.0f) * 255);
    }
}
/**
 * @brief Build a monochrome colormap with gamma correction for better contrast
 */
void SonarPane::buildSonarMonochrommap(uint8_t colormap[256][3]) {
    for (int i = 0; i < 256; ++i) {
        // apply gamma for better contrast
        float gamma = 1.5f;
        colormap[i][0] = static_cast<uint8_t>(std::pow(i / 255.0f, gamma) * 255.0f);
        colormap[i][1] = colormap[i][0];
        colormap[i][2] = colormap[i][0];
    }
}

void SonarPane::OnResize(wxSizeEvent& event) {
    Refresh(false);
    _x = event.GetSize().GetX();
    _y = event.GetSize().GetY();
}
#define BLINDZONE_SAMPLE_END NUM_SAMPLES / MAX_DEPTH * .5

void SonarPane::Render(wxPaintEvent& event) {
    if (!m_parent->m_pi->IsOpenGLEnabled()) return;

    // This is required even though dc is not used otherwise.
    wxPaintDC dc(this);

    SetCurrent(*m_context);

    GLfloat _h = 2.0 / (GLfloat)(NUM_SAMPLES - BLINDZONE_SAMPLE_END);  // skip blind zone

    glPushMatrix();
    glPushAttrib(GL_ALL_ATTRIB_BITS);
#if (wxCHECK_VERSION(3, 1, 6))
    glClearColor((GLfloat)(m_parent->m_pi->m_background_colour.GetRed() / 256.0),
                 (GLfloat)(m_parent->m_pi->m_background_colour.GetGreen() / 256.0),
                 (GLfloat)(m_parent->m_pi->m_background_colour.GetBlue() / 256.0), 1.0f);
#else
    glClearColor((GLfloat)(m_parent->m_pi->m_background_colour.Red() / 256.0),
                 (GLfloat)(m_parent->m_pi->m_background_colour.Green() / 256.0),
                 (GLfloat)(m_parent->m_pi->m_background_colour.Blue() / 256.0), 1.0f);
#endif
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_BLEND);
    glEnable(GL_MULTISAMPLE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0, 0, _x, _y);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);


    /**
     * Get mean and standard deviation to adjust gain automatically
     */

    uint32_t sum = 0;
    uint32_t count = 0;
    for (uint16_t i = m_parent->index; i < BUF_X; i++) {
        for (uint16_t j = HEADER_SIZE; j < NUM_SAMPLES + HEADER_SIZE; j++) {
            sum += data_buffer[i][j];
            count++;
        }
    }

    // Calculate mean and standard deviation
    float mean = (float)sum / (float)count;
    float variance = 0.0f;
    for (uint16_t i = m_parent->index; i < BUF_X; i++) {
        for (uint16_t j = HEADER_SIZE; j < NUM_SAMPLES + HEADER_SIZE; j++) {
            float diff = (float)data_buffer[i][j] - mean;
            variance += diff * diff;
        }
    }

    // Finalize variance calculation
    variance /= count;
    float sigma = sqrt(variance);
    double minLevel = mean - 2.0 * sigma;
    double maxLevel = mean + 2.0 * sigma;
    if (maxLevel <= minLevel) {
        minLevel = mean - 1.0;
        maxLevel = mean + 1.0;
    }
    double range = maxLevel - minLevel;  // Not const, needed for debug output later
    const double invRange = (range > 0.0) ? (255.0 / range) : 0.0;

    // Pre-compute lookup tables for normalization and color mapping
    // This avoids repeated calculations for each pixel
    static uint16_t colorLUT[256];

    for (int v = 0; v < 256; ++v) {
        double norm = (static_cast<double>(v) - minLevel) * invRange;
        if (norm < 0.0) norm = 0.0;
        if (norm > 255.0) norm = 255.0;
        uint8_t idx = static_cast<uint8_t>(norm + 0.5);
        colorLUT[v] = colorMap[idx];
    }

    /**
     * Render the sonar data
     */

    for (uint16_t i = 0; i < BUF_X; i++) {
        for (uint16_t j = HEADER_SIZE + BLINDZONE_SAMPLE_END; j < NUM_SAMPLES + HEADER_SIZE; j++) {
            // Only display the data that has been received
            if (i >= m_parent->index) {
                uint16_t rgb565 = colorLUT[data_buffer[i][j]];

                uint8_t r5 = (rgb565 >> 11) & 0x1F;
                uint8_t g6 = (rgb565 >> 5) & 0x3F;
                uint8_t b5 = rgb565 & 0x1F;

                uint8_t r8 = (r5 << 3) | (r5 >> 2);
                uint8_t g8 = (g6 << 2) | (g6 >> 4);
                uint8_t b8 = (b5 << 3) | (b5 >> 2);

                GLfloat R = (GLfloat)(r8) / (GLfloat)255.0;
                GLfloat G = (GLfloat)(g8) / (GLfloat)255.0;
                GLfloat B = (GLfloat)(b8) / (GLfloat)255.0;

                glColor4f(R, G, B, 1.0f);
            } else {
                // Let the background shine through for unreceived data
                glColor4f(0.0f, 0.0f, 0.0f, 0.0f);
            }

            GLfloat h1 = ((GLfloat)(j - HEADER_SIZE - BLINDZONE_SAMPLE_END) * _h - 1.0) * -1.0;
            GLfloat h2 = ((GLfloat)(j - HEADER_SIZE - BLINDZONE_SAMPLE_END) * _h + _h - 1.0) * -1.0;

            GLfloat w1 = (GLfloat)i * _w - 1.0;
            GLfloat w2 = (GLfloat)i * _w + _w - 1.0;

            glRectf(w1, h1, w2, h2);
        }
    }

    glPopAttrib();
    glPopMatrix();

    SwapBuffers();
}

PLUGIN_END_NAMESPACE
