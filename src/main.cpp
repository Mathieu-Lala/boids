#include <iostream>

#include <imgui.h>
#include <imgui-SFML.h>

#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/System/Clock.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Graphics/CircleShape.hpp>

#include <entt/entt.hpp>
#include <glm/glm.hpp>

struct Drawable {
    sf::Shape *shape{nullptr};

    static auto on_destroy(entt::registry &world, entt::entity entity)
    {
        delete world.get<Drawable>(entity).shape;
    }
};

struct Position {
    glm::vec2 vec{0, 0};

    static auto on_update(entt::registry &world, entt::entity entity)
    {
        if (world.has<Drawable>(entity)) {
            const auto &pos = world.get<Position>(entity).vec;
            world.get<Drawable>(entity).shape->setPosition({pos.x, pos.y});
        }
    }
};

struct Velocity {
    glm::vec2 vec{0, 0};
};

struct Orientation {
    float value{0};

    static auto on_update(entt::registry &world, entt::entity entity)
    {
        if (world.has<Drawable>(entity)) {
            const auto &ori = world.get<Orientation>(entity).value;
            world.get<Drawable>(entity).shape->setRotation(ori + 90);
        }
    }
};

struct Context {
    std::int32_t object_count{10};
    float object_size{50};
    float velocity_scalar{1.0f};
    float contact_distance{1};
    float close_distance{1};
};

int main()
{
    std::srand(static_cast<std::uint32_t>(std::time(nullptr)));

    constexpr auto WINDOW_W = 640;
    constexpr auto WINDOW_H = 480;

    sf::RenderWindow window(sf::VideoMode(WINDOW_W, WINDOW_H), "ImGui + SFML = <3");
    window.setFramerateLimit(60);
    ImGui::SFML::Init(window);

    entt::registry world;
    world.on_destroy<Drawable>().connect<Drawable::on_destroy>();
    world.on_update<Position>().connect<Position::on_update>();
    world.on_construct<Position>().connect<Position::on_update>();
    world.on_update<Orientation>().connect<Orientation::on_update>();
    world.on_construct<Orientation>().connect<Orientation::on_update>();

    Context context;

    const auto create_scene = [&world, &context]() {
        world.clear();
        for (int i = 0; i != context.object_count; i++) {
            const auto e = world.create();
            const auto drawable = new sf::CircleShape{context.object_size};
            drawable->setFillColor(sf::Color::Green);
            drawable->setOrigin({context.object_size, context.object_size});
            drawable->setPointCount(3);
            world.emplace<Drawable>(e, drawable);
            world.emplace<Position>(
                e,
                glm::vec2{
                    std::rand() % static_cast<std::int32_t>(WINDOW_W - context.object_size * 2),
                    std::rand() % static_cast<std::int32_t>(WINDOW_H - context.object_size * 2)});
            world.emplace<Velocity>(e);
            world.emplace<Orientation>(e, std::rand() % 360);
        }
        context.contact_distance = context.object_size * 1.3f;
        context.close_distance = context.object_size * 3.9f;
    };

    create_scene();

    sf::Clock deltaClock;
    while (window.isOpen()) {
        const auto elapsedTime = deltaClock.restart();
        // const auto et_as_ms = static_cast<float>(elapsedTime.asMilliseconds());

        sf::Event event;
        while (window.pollEvent(event)) {
            ImGui::SFML::ProcessEvent(event);

            if (event.type == sf::Event::Closed) { window.close(); }
            if (event.type == sf::Event::KeyPressed) {
                if (event.key.code == sf::Keyboard::Escape) { window.close(); }
            }
        }

        // apply the velocity to the position
        {
            const auto view = world.view<Position, Velocity>();
            for (const auto &e : view) {
                const auto &vel = view.get<Velocity>(e);
                world.patch<Position>(
                    e, [&vel, &context](auto &pos) { pos.vec += vel.vec * context.velocity_scalar; });
            }
        }

        // apply the orientation to the velocity
        world.view<Velocity, Orientation>().each([](auto &vel, auto &rot) {
            vel.vec = {std::cos(glm::radians(rot.value)), std::sin(glm::radians(rot.value))};
        });

        // make the object traverse the border
        {
            for (const auto &e : world.view<Position>()) {
                const auto &obj_pos = world.get<Position>(e);
                const auto LIMIT_LEFT = context.object_size;
                const auto LIMIT_RIGHT = WINDOW_W - context.object_size;
                const auto LIMIT_UP = context.object_size;
                const auto LIMIT_DOWN = WINDOW_H - context.object_size;

                if (obj_pos.vec.x < LIMIT_LEFT - context.object_size * 2.0f) {
                    world.patch<Position>(e, [&LIMIT_RIGHT, &context](auto &pos) {
                        pos.vec.x = LIMIT_RIGHT + context.object_size * 2.0f;
                    });
                }
                if (obj_pos.vec.x > LIMIT_RIGHT + context.object_size * 2.0f) {
                    world.patch<Position>(e, [&LIMIT_LEFT, &context](auto &pos) {
                        pos.vec.x = LIMIT_LEFT - context.object_size * 2.0f;
                    });
                }
                if (obj_pos.vec.y < LIMIT_UP - context.object_size * 2.0f) {
                    world.patch<Position>(e, [&LIMIT_DOWN, &context](auto &pos) {
                        pos.vec.y = LIMIT_DOWN + context.object_size * 2.0f;
                    });
                }
                if (obj_pos.vec.y > LIMIT_DOWN + context.object_size * 2.0f) {
                    world.patch<Position>(e, [&LIMIT_UP, &context](auto &pos) {
                        pos.vec.y = LIMIT_UP - context.object_size * 2.0f;
                    });
                }
            }
        }

        // update the color depending of distance to each others
        {
            const auto view = world.view<Position, Drawable>();
            for (const auto &ii : view) {
                const auto &pos_ii = world.get<Position>(ii);

                auto min_distance = std::numeric_limits<float>::max();

                for (const auto &i : view) {
                    if (i == ii) continue;

                    const auto &pos_i = world.get<Position>(i);
                    const auto distance = glm::distance(pos_i.vec, pos_ii.vec);
                    min_distance = std::min(min_distance, distance);
                }

                const auto &drawable_ii = world.get<Drawable>(ii);
                if (min_distance <= context.contact_distance) {
                    drawable_ii.shape->setFillColor(sf::Color::Red);
                } else if (min_distance <= context.close_distance) {
                    drawable_ii.shape->setFillColor(sf::Color::Yellow);
                } else {
                    drawable_ii.shape->setFillColor(sf::Color::Green);
                }
            }
        }

        const auto get_angle = [](const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 origin) {
            const auto da = glm::normalize(a - origin);
            const auto db = glm::normalize(b - origin);
            return glm::acos(glm::dot(da, db));
        };

        const auto get_is_on_right = [](const glm::vec2 &a, const glm::vec2 &b, const glm::vec2 &dir) {
            const auto direction = glm::normalize(dir);
            const auto right =
                glm::cross(glm::vec3{direction.x, direction.y, 1.0f}, glm::vec3{1.0f, 0.0f, 1.0f});

            const auto other = a - b;

            const auto dot = glm::dot(right, glm::vec3{other.x, other.y, 1.0});
            return dot < 0;
        };

        // avoid collision
        {
            const auto view = world.view<Position, Velocity, Orientation>();
            for (const auto &ii : view) {
                const auto &pos_ii = world.get<Position>(ii);
                const auto &vel_ii = world.get<Velocity>(ii);

                for (const auto &i : view) {
                    if (i == ii) continue;

                    const auto &pos_i = world.get<Position>(i);
                    const auto distance = glm::distance(pos_i.vec, pos_ii.vec);

                    if (distance <= context.close_distance) {
                        const auto angle = glm::degrees(
                            get_angle(pos_ii.vec + glm::normalize(vel_ii.vec), pos_i.vec, pos_ii.vec));
                        const auto is_on_right = get_is_on_right(pos_ii.vec, pos_i.vec, vel_ii.vec);

                        world.patch<Orientation>(i, [&angle, &is_on_right](auto &rot) {
                            rot.value += angle / 100.0f * (is_on_right ? 1 : -1);
                        });
                    }
                }
            }
        }

        /*
        // update the rotation depending of the close boids
        {
            std::unordered_map<entt::entity, float> new_rot;
            const auto view = world.view<Rotation, Position>();
            for (const auto &ii : view) {
                const auto &rot_ii = world.get<Rotation>(ii);
                const auto &pos_ii = world.get<Position>(ii);

                float rot_count = 1.0f;
                float rot_sum = rot_ii.value;

                for (const auto &i : view) {
                    if (i == ii) continue;

                    const auto &pos_i = world.get<Position>(i);
                    const auto distance = glm::distance(pos_i.vec, pos_ii.vec);
                    if (distance >= context.close_distance) continue;

                    const auto &rot_i = world.get<Rotation>(i);

                    rot_count++;
                    rot_sum += rot_i.value;
                }

                new_rot[ii] = rot_sum / rot_count;
            }
            for (const auto &i : view) {
                world.patch<Rotation>(i, [value = new_rot[i]](auto &rot) { rot.value = value; });
            }
        }
        */


        ImGui::SFML::Update(window, elapsedTime);

        ImGui::Begin("Context");
        if (ImGui::DragFloat("Object Size", &context.object_size, 1.0f, 1.0f, 300.0f)) { create_scene(); }
        if (ImGui::DragInt("Object Count", &context.object_count, 1.0f, 1.0f, 300.0f)) { create_scene(); }
        ImGui::Separator();
        ImGui::DragFloat(
            "Velocity scalar", &context.velocity_scalar, 1.0f / 10.0f, 1.0f / 10.0f, 10.0f, "%.4f", ImGuiSliderFlags_Logarithmic);
        ImGui::Separator();
        ImGui::DragFloat("Close Distance", &context.close_distance, 1.0f, context.contact_distance, 1000.0f);
        ImGui::DragFloat("Contact Distance", &context.contact_distance, 1.0f, 0.0f, context.close_distance);
        ImGui::End();

        window.clear();
        world.view<Drawable>().each([&window](auto &drawable) { window.draw(*drawable.shape); });
        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();

    world.clear();

    return 0;
}
