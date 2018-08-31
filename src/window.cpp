#include "window.h"
#include "screen.h"

#if defined(__GNUC__) || defined(__clang__)
#    pragma GCC diagnostic ignored "-Wunused-parameter"
#endif

using namespace nanogui;

NanoVisWindow::NanoVisWindow(const std::string &title, int width, int height) {
    m_screen = new NanoVisScreen(this, Vector2i(width, height), title);

    viewport_ryp = {0, -45, -42};
    viewport_xyz = {-8, -8, 8};
    viewport_scale = 1.0;
}

NanoVisWindow::~NanoVisWindow() = default;

void NanoVisWindow::show() {
    m_screen->setVisible(true);
    m_screen->performLayout();
    refresh();
}

void NanoVisWindow::refresh() {
    for (const auto &s : m_subscribers) {
        broadcast(s.first);
    }
    m_screen->drawAll();
}

int NanoVisWindow::width() const {
    return m_screen->width();
}

int NanoVisWindow::height() const {
    return m_screen->height();
}

void NanoVisWindow::add_button(const std::string &title, const std::string &name, const std::function<void()> &callback) {
    Button *b = new Button(panel(title), name);
    b->setCallback(callback);
    add_widget(title, b);
}

void NanoVisWindow::add_toggle(const std::string &title, const std::string &name, const std::function<void(bool)> &callback) {
    Button *b = new Button(panel(title), name);
    b->setFlags(Button::Flags::ToggleButton);
    b->setChangeCallback(callback);
    add_widget(title, b);
}

void NanoVisWindow::add_toggle(const std::string &title, const std::string &name, bool &value, const std::function<void(bool)> &callback) {
    Button *b = new Button(panel(title), name);
    b->setFlags(Button::Flags::ToggleButton);

    auto subscriber = [&value, b]() {
        if (b->pushed() != value) {
            b->setPushed(value);
        }
    };
    subscriber();
    m_subscribers[(void *)(&value)][b] = subscriber;

    b->setChangeCallback([&value, callback, this](bool new_value) {
        if (value != new_value) {
            value = new_value;
            this->notify(value);
            if (callback) {
                callback(new_value);
            }
        }
    });
    add_widget(title, b);
}

void NanoVisWindow::add_repeat(const std::string &title, const std::string &name, const std::function<bool()> &callback) {
    Button *b = new Button(panel(title), name);
    b->setFlags(Button::Flags::ToggleButton);
    if (callback) {
        b->setChangeCallback([b, callback, this](bool pushed) {
            if (pushed) {
                // clang-format off
                m_screen->setInterval([b, callback]() {
                    if (b->pushed()) {
                        if (!callback()) {
                            b->setPushed(false);
                        }
                    }
                    return b->pushed();
                }, 1);
                // clang-format on
            }
        });
    }
    add_widget(title, b);
}

void NanoVisWindow::add_graph(const std::string &title, const std::string &name, double &value, const double &max_value, const double &min_value) {
    Graph *g = new Graph(panel(title), name);

    VectorXf &f = g->values();
    f.resize(60);
    for (int i = 0; i < f.rows(); ++i) {
        f[i] = (value - min_value) / (max_value - min_value);
    }

    auto subscriber = [&value, g, max_value, min_value]() {
        VectorXf &f = g->values();
        for (int i = 1; i < f.rows(); ++i) {
            f[i - 1] = f[i];
        }
        f[f.rows() - 1] = (value - min_value) / (max_value - min_value);
        g->setFooter(std::to_string(value));
    };
    subscriber();
    m_subscribers[(void *)(&value)][g] = subscriber;

    add_widget(title, g);
}

void NanoVisWindow::add_graph(const std::string &title, const std::string &name, std::vector<double> &values, const double &max_value, const double &min_value) {
    Graph *g = new Graph(panel(title), name);

    auto subscriber = [&values, g, max_value, min_value]() {
        VectorXf &f = g->values();
        f.resize(values.size());
        double computed_max_value = -std::numeric_limits<double>::max();
        double computed_min_value = +std::numeric_limits<double>::max();
        if (max_value <= min_value) {
            for (size_t i = 0; i < values.size(); ++i) {
                computed_max_value = std::max(computed_max_value, values[i]);
                computed_min_value = std::min(computed_min_value, values[i]);
            }
        } else {
            computed_max_value = max_value;
            computed_min_value = min_value;
        }
        for (size_t i = 0; i < values.size(); ++i) {
            f[i] = (values[i] - computed_min_value) / std::max((computed_max_value - computed_min_value), std::numeric_limits<double>::epsilon());
        }
    };
    subscriber();
    m_subscribers[(void *)(&values)][g] = subscriber;

    add_widget(title, g);
}

void NanoVisWindow::add_image(const std::string &title, const std::string &name, cv::Mat &image) {
    GLuint texture_id;
    glGenTextures(1, &texture_id); // well okay I admit that this handle is never released in program.
    ImageView *i = new ImageView(panel(title), texture_id);

    i->setFixedSize({240, 180});
    i->setFixedScale(true);
    i->setFixedOffset(true);

    auto subscriber = [&image, i, texture_id]() {
        if (image.empty()) return;
        cv::Mat rgb_image;
        if (image.channels() == 1) {
            cv::cvtColor(image, rgb_image, cv::COLOR_GRAY2BGR);
        } else {
            rgb_image = image;
        }
        glBindTexture(GL_TEXTURE_2D, texture_id);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, rgb_image.cols, rgb_image.rows, 0, GL_BGR, GL_UNSIGNED_BYTE, rgb_image.ptr());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

        i->bindImage(texture_id);
    };
    subscriber();
    m_subscribers[(void *)(&image)][i] = subscriber;

    add_widget(title, i);
}

void NanoVisWindow::add_widget(const std::string &title, Widget *widget) {
    AdvancedGridLayout *l = layout(title);
    if (l->rowCount() > 0) {
        l->appendRow(3);
    }
    l->appendRow(0);
    l->setAnchor(widget, AdvancedGridLayout::Anchor(0, l->rowCount() - 1, l->colCount(), 1));
}

void NanoVisWindow::add_widget(const std::string &title, Widget *widget_left, Widget *widget_right) {
    AdvancedGridLayout *l = layout(title);
    if (l->rowCount() > 0) {
        l->appendRow(3);
    }
    l->appendRow(0);
    if (widget_left) {
        l->setAnchor(widget_left, AdvancedGridLayout::Anchor(0, l->rowCount() - 1, 1, 1));
    }
    if (widget_right) {
        l->setAnchor(widget_right, AdvancedGridLayout::Anchor(2, l->rowCount() - 1, 1, 1));
    }
}

Window *NanoVisWindow::panel(const std::string &title) {
    if (m_panels.count(title) == 0) {
        AdvancedGridLayout *l = new AdvancedGridLayout({60, 3, 120}, {});
        l->setMargin(5);
        l->setColStretch(2, 1);

        Window *w = new Window(m_screen, title);
        w->setLayout(l);
        w->setVisible(true);

        m_panels.emplace(title, w);
    }
    return m_panels.at(title);
}

AdvancedGridLayout *NanoVisWindow::layout(const std::string &title) {
    return static_cast<AdvancedGridLayout *>(panel(title)->layout());
}

void NanoVisWindow::draw() {
}

bool NanoVisWindow::mouseButtonEvent(const Vector2i &p, int button, bool down, int modifiers) {
    viewport_xyz_old = viewport_xyz;
    viewport_ryp_old = viewport_ryp;
    viewport_cursor_old = p;
    if (down) {
        if (button == 0) {
            viewport_translation_mode = true;
            viewport_rotation_mode = false;
        } else if (button == 1) {
            viewport_rotation_mode = true;
            viewport_translation_mode = false;
        }
    } else {
        viewport_translation_mode = viewport_rotation_mode = false;
    }
    return true;
}

bool NanoVisWindow::mouseMotionEvent(const Vector2i &p, const Vector2i &rel, int button, int modifiers) {
    Eigen::Vector2f delta = (p - viewport_cursor_old).cast<float>() * 0.1;
    if (viewport_rotation_mode) {
        viewport_ryp.segment<2>(1) = viewport_ryp_old.segment<2>(1) + delta;
    } else if (viewport_translation_mode) {
        Eigen::Matrix3f world_view_dcm = (view_matrix() * world_matrix()).block<3, 3>(0, 0).transpose();
        Eigen::Vector3f pos = viewport_xyz_old;
        if (modifiers == 0) {
            pos = viewport_xyz_old - world_view_dcm.block<3, 2>(0, 0) * delta;
        } else if (modifiers == 1) {
            Eigen::Vector3f dz = world_view_dcm.col(2);
            dz.z() = 0;
            dz.normalize();
            pos = pos - world_view_dcm.col(0) * delta.x() + dz * delta.y();
        }
        viewport_xyz = pos;
    }
    return true;
}

bool NanoVisWindow::mouseDragEvent(const Vector2i &p, const Vector2i &rel, int button, int modifiers) {
    return true;
}

bool NanoVisWindow::mouseEnterEvent(const Vector2i &p, bool enter) {
    return true;
}

bool NanoVisWindow::mouseScrollEvent(const Vector2i &p, const Vector2f &rel) {
    viewport_scale += rel.y() * 0.01;
    return true;
}

Eigen::Matrix4f NanoVisWindow::proj_matrix(float near, float far) const {
    Eigen::Matrix4f proj = Eigen::Matrix4f::Zero();
    float f = height();
    proj(0, 0) = 2 * f / width();
    proj(1, 1) = -2 * f / height();
    proj(2, 2) = (far + near) / (far - near);
    proj(2, 3) = 2 * far * near / (near - far);
    proj(3, 2) = 1.0;
    return proj;
}

Eigen::Matrix4f NanoVisWindow::view_matrix() const {
    Eigen::Matrix4f view = Eigen::Matrix4f::Zero();
    view(0, 0) = 1.0;
    view(2, 1) = 1.0;
    view(1, 2) = -1.0;
    view(3, 3) = 1.0;
    return view;
}

Eigen::Matrix4f NanoVisWindow::world_matrix() const {
    Eigen::Matrix3f R = Eigen::Matrix3f::Identity();
    R = Eigen::AngleAxisf(viewport_ryp[0] * M_PI / 180.0f, Eigen::Vector3f::UnitY()) * R;
    R = Eigen::AngleAxisf(viewport_ryp[2] * M_PI / 180.0f, Eigen::Vector3f::UnitX()) * R;
    R = Eigen::AngleAxisf(viewport_ryp[1] * M_PI / 180.0f, Eigen::Vector3f::UnitZ()) * R;

    Eigen::Matrix4f world = Eigen::Matrix4f::Zero();
    world.block<3, 3>(0, 0) = R.transpose();
    world.block<3, 1>(0, 3) = -R.transpose() * viewport_xyz;
    world(3, 3) = 1.0;

    return world;
}

// Eigen::Matrix4f NanoVisWindow::model_view_proj() const {
//     return proj_matrix() * view_matrix() * world_matrix();
// }

float NanoVisWindow::world_scale() const {
    return viewport_scale;
}

void NanoVisWindow::broadcast(const void *value) {
    if (m_subscribers.count(value) > 0) {
        for (const auto &s : m_subscribers.at(value)) {
            if (s.second) {
                s.second();
            }
        }
    }
}