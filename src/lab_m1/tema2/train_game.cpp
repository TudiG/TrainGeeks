#include "train_game.h"

#include <iostream>
#include <algorithm>

using namespace std;
using namespace m1;

/* =========================================================
 *  Constructor / Destructor
 * ========================================================= */
TrainGame::TrainGame() {}
TrainGame::~TrainGame() {
    delete textRenderer;
}

/* =========================================================
 *  Init
 * ========================================================= */
void TrainGame::Init()
{
    renderCameraTarget = false;

    camera = new implemented::Camera();
    camera->Set(glm::vec3(0, 3, 5), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));

    // Meshes
    {
        Mesh* mesh = new Mesh("box");
        mesh->LoadMesh(PATH_JOIN(window->props.selfDir,
            RESOURCE_PATH::MODELS, "primitives"), "box.obj");
        meshes[mesh->GetMeshID()] = mesh;
    }
    {
        Mesh* mesh = new Mesh("sphere");
        mesh->LoadMesh(PATH_JOIN(window->props.selfDir,
            RESOURCE_PATH::MODELS, "primitives"), "sphere.obj");
        meshes[mesh->GetMeshID()] = mesh;
    }
    {
        Mesh* mesh = new Mesh("pyramid");
        mesh->LoadMesh(PATH_JOIN(window->props.selfDir,
            RESOURCE_PATH::MODELS, "primitives"), "pyramid.obj");
        meshes[mesh->GetMeshID()] = mesh;
    }
    {
        Mesh* mesh = new Mesh("cylinder");
        mesh->LoadMesh(PATH_JOIN(window->props.selfDir,
            RESOURCE_PATH::MODELS, "primitives"), "cylinder.obj");
        meshes[mesh->GetMeshID()] = mesh;
    }
    // end

    // Shader
    {
        Shader* shader = new Shader("TrainShader");

        shader->AddShader(
            PATH_JOIN(window->props.selfDir,
                SOURCE_PATH::M1,
                "tema2", "shaders", "VertexShader.glsl"),
            GL_VERTEX_SHADER);

        shader->AddShader(
            PATH_JOIN(window->props.selfDir,
                SOURCE_PATH::M1,
                "tema2", "shaders", "FragmentShader.glsl"),
            GL_FRAGMENT_SHADER);

        shader->CreateAndLink();
        shaders[shader->GetName()] = shader;
    }
    // end

    // Camera and projection
    isPerspective = true;
    fov = RADIANS(60);
    aspect = window->props.aspectRatio;
    zNear = 0.01f;
    zFar = 200.0f;

    left = -5; right = 5;
    bottom = -5; top = 5;

    cameraSpeed = 2.0f;
    sensivityOX = sensivityOY = 0.001f;

    projectionMatrix = glm::perspective(fov, aspect, zNear, zFar);
    // end

    // game Init
    int di[4] = { -1, 0, 1, 0 };
    int dj[4] = { 0, 1, 0, -1 };

    grid.clear();
    gridTrains.clear();
    stations.clear();
    stationFullnessTimers.clear();
    selectedStation = -1;
    gameOver = circleExists = squareExists = pyramidExists = false;
    totalDeliveredPassengers = 0;
    currentPoints = 15;
    gameTime = 0.0f;

    auto resolution = window->GetResolution();
    textRenderer = new gfxc::TextRenderer(window->props.selfDir, resolution.x, resolution.y);
    textRenderer->Load(PATH_JOIN(window->props.selfDir, RESOURCE_PATH::FONTS, "Hack-Bold.ttf"), 48);
    // end

    // Grid Init
    InitGrid();

    SpawnRandomStation();
    SpawnRandomStation();
    // end
}

/* =========================================================
 *  Grid init
 * ========================================================= */
void TrainGame::InitGrid()
{
    grid.assign(GRID_H, std::vector<Cell>(GRID_W));

    // Grass
    for (int i = 0; i < GRID_H; i++) {
        for (int j = 0; j < GRID_W; j++) {
            grid[i][j].type = CellType::Grass;
            grid[i][j].hasStation = false;
        }
    }
    // end

    // River
    int i = GRID_H / 2;
    int j = 0;
    while (j < GRID_W) {
        grid[i][j].type = CellType::Water;
        grid[i - 1][j].type = CellType::Water;
        grid[i + 1][j].type = CellType::Water;

        int dirChance = rand() % 3;
        if (dirChance == 0 && i > 1) i--;
        else if (dirChance == 2 && i < GRID_H - 2) i++;

        j++;
    }
    // end

    // Mountains
    int mountaiCount = 1 + rand() % 5;
    for (int k = 0; k < mountaiCount; k++) {
        int centerI = rand() % GRID_H;
        int centerJ = rand() % GRID_W;

        int radius = 3 + rand() % 4;
        float coreRadius = radius * 0.6f;

        for (int i = centerI - radius; i <= centerI + radius; i++) {
            for (int j = centerJ - radius; j <= centerJ + radius; j++) {
                if (i < 0 || j < 0 || i >= GRID_H || j >= GRID_W) continue;
                if (grid[i][j].type == CellType::Water) continue;

                float di = float(i - centerI);
                float dj = float(j - centerJ);
                float dist = sqrtf(di * di + dj * dj);

                if (dist > radius) continue;

                if (dist <= coreRadius) {
                    grid[i][j].type = CellType::Mountain;
                }
                else {
                    float t = (dist - coreRadius) / (radius - coreRadius);
                    float chance = 1.0f - t;
                    float r = float(rand()) / RAND_MAX;
                    if (r < chance) grid[i][j].type = CellType::Mountain;
                }
            }
        }
    }
    // end

    // Mountain Smoothing
    std::vector<std::vector<Cell>> copy = grid;
    for (int i = 1; i < GRID_H - 1; i++) {
        for (int j = 1; j < GRID_W - 1; j++) {
            if (grid[i][j].type == CellType::Water) continue;

            int mountainCellCount = 0;
            if (grid[i - 1][j].type == CellType::Mountain) mountainCellCount++;
            if (grid[i + 1][j].type == CellType::Mountain) mountainCellCount++;
            if (grid[i][j - 1].type == CellType::Mountain) mountainCellCount++;
            if (grid[i][j + 1].type == CellType::Mountain) mountainCellCount++;

            if (mountainCellCount >= 3) copy[i][j].type = CellType::Mountain;
            else if (mountainCellCount <= 1) copy[i][j].type = CellType::Grass;
        }
    }

    grid = copy;
    // end
}

/* =========================================================
 *  Grid helpers
 * ========================================================= */
glm::vec3 TrainGame::CellToWorld(int i, int j) const
{
    float x = (j - GRID_W / 2) * CELL_SIZE + CELL_SIZE * 0.5f;
    float z = (i - GRID_H / 2) * CELL_SIZE + CELL_SIZE * 0.5f;
    return glm::vec3(x, 0.0f, z);
}

bool TrainGame::WorldToCell(const glm::vec3& p, int& i, int& j) const
{
    j = int((p.x + GRID_W * CELL_SIZE / 2) / CELL_SIZE);
    i = int((p.z + GRID_H * CELL_SIZE / 2) / CELL_SIZE);
    return !(i < 0 || j < 0 || i >= GRID_H || j >= GRID_W);
}


/* =========================================================
 *  Frame Start/End + Update
 * ========================================================= */
void TrainGame::FrameStart()
{
    glClearColor(0.12f, 0.14f, 0.16f, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::ivec2 res = window->GetResolution();
    glViewport(0, 0, res.x, res.y);
}

void TrainGame::Update(float dt)
{
    // ***** FAIL STATE DETECTION *****
    if (gameOver) {
        std::string text1 = "GAME OVER";
        std::string text2 = "Passengers delivered: " + std::to_string(totalDeliveredPassengers);
        std::string text3 = "Press SPACE to try again";

        auto resolution = window->GetResolution();

        textRenderer->RenderText(text1, 420, 260, 1.5f, glm::vec3(1, 0, 0));
        textRenderer->RenderText(text2, 330, 340, 1.0f, glm::vec3(1, 1, 1));
        textRenderer->RenderText(text3, 445, 420, 0.5f, glm::vec3(1, 1, 1));
        return;
    }

    if (stationFullnessTimers.size() != stations.size()) {
        stationFullnessTimers.resize(stations.size(), 0.0f);
    }

    bool allStationsSafe = true;
    for (size_t i = 0; i < stations.size(); i++) {
        bool isFull = (stations[i].waitingPassengers.size() >= 10);
        if (isFull) {
            stationFullnessTimers[i] += dt;

            if (stationFullnessTimers[i] > stationMaxFullnessTimer) {
                allStationsSafe = false;
            }
        }
        else {
            stationFullnessTimers[i] = 0.0f;
        }
    }

    if (!allStationsSafe) {
        gameOver = true;
    }

    // ***** SCORE AND INFO TEXT *****
    std::string pointsText = "Points: " + std::to_string(currentPoints);
    textRenderer->RenderText(pointsText, 10, 10, 0.5f, glm::vec3(1, 1, 1));

    gameTime += dt;
    int minutes = (int)gameTime / 60;
    int seconds = (int)gameTime % 60;
    std::string timeText = "Time: " + std::to_string(minutes) + ":" + std::to_string(seconds);
    textRenderer->RenderText(timeText, 10, 50, 0.5f, glm::vec3(1, 1, 1));

    // ***** SPAWN STATION *****
    stationSpawnTimer += dt;
    if (stationSpawnTimer >= stationSpawnInterval) {
        stationSpawnTimer = 0.0f;
        SpawnRandomStation();
    }

    // ***** UPDATE PASSANGERS *****
    static float passengerTimer = 0.0f;
    passengerTimer += dt;
    if (passengerTimer > passengerSpawnInterval)
    {
        for (const auto& s : stations)
        {
            if (s.shape == StationShape::Circle) circleExists = true;
            if (s.shape == StationShape::Square) squareExists = true;
            if (s.shape == StationShape::Pyramid) pyramidExists = true;
        }

        passengerTimer = 0.0f;

        for (auto& s : stations)
        {
            if ((int)s.waitingPassengers.size() >= 10) continue;

            Passenger p;
            int r = rand() % 3;
            p.type = (StationShape)r;

            if (p.type == s.shape) continue;
            
            if (p.type == StationShape::Pyramid && pyramidExists
                || p.type == StationShape::Circle && circleExists
                || p.type == StationShape::Square && squareExists)
                s.waitingPassengers.push_back(p);
        }
    }

    // ***** UPDATE TRAINS *****
    UpdateGridTrains(dt);

    // ***** GRID RENDER *****
    RenderGrid();
    RenderGridRails();

    // ***** TRAIN AND WAGON RENDER *****
    for (auto& t : gridTrains)
    {
        if (t.trail.size() < 2) continue;

        glm::vec3 locoPos = t.trail[0];
        glm::vec3 locoDir = glm::normalize(t.trail[0] - t.trail[1]);

        RenderLocomotive(locoPos, locoDir);

        float distAccum = 0.f;
        int wagonIndex = 0;
        float targetDist = locomotiveLength;

        for (int k = 1; k < (int)t.trail.size() && wagonIndex < t.wagons; k++)
        {
            float d = glm::distance(t.trail[k - 1], t.trail[k]);
            distAccum += d;

            if (distAccum >= targetDist)
            {
                glm::vec3 wagonPos = t.trail[k];
                glm::vec3 wagonDir = glm::normalize(t.trail[k - 1] - t.trail[k]);

                RenderWagon(wagonPos, wagonDir);
                RenderWagonPassengers(t, wagonIndex, wagonPos, wagonDir);

                wagonIndex++;
                targetDist += wagonSpacing;
            }
        }
    }

    // ***** STATIONS *****
    for (auto& s : stations) {
        RenderStation(s);
        RenderStationPassengers(s);
    }
}

void TrainGame::FrameEnd()
{
    //DrawCoordinateSystem(camera->GetViewMatrix(), projectionMatrix);
}

/* =========================================================
 * Rendering
 * ========================================================= */
void TrainGame::RenderGrid()
{
    const glm::vec3 grass(0.45f, 0.70f, 0.45f);
    const glm::vec3 water(0.20f, 0.40f, 0.80f);
    const glm::vec3 mountain(0.55f, 0.55f, 0.55f);

    for (int i = 0; i < GRID_H; i++) {
        for (int j = 0; j < GRID_W; j++) {
            glm::vec3 pos = CellToWorld(i, j);

            glm::vec3 color = grass;
            if (grid[i][j].type == CellType::Water) color = water;
            else if (grid[i][j].type == CellType::Mountain) color = mountain;

            glm::mat4 m(1);
            m = glm::translate(m, pos + glm::vec3(0, -0.02f, 0));
            m = glm::scale(m, glm::vec3(CELL_SIZE, 0.02f, CELL_SIZE));

            RenderMeshColor(meshes["box"], m, color);
        }
    }
}

void TrainGame::RenderGridRails()
{
    const glm::vec3 normalColor(0.8f, 0.8f, 0.8f);
    const glm::vec3 bridgeColor(0.9f, 0.9f, 0.6f);
    const glm::vec3 tunnelColor(0.35f, 0.35f, 0.35f);

    for (int i = 0; i < GRID_H; i++) {
        for (int j = 0; j < GRID_W; j++) {

            unsigned char m = grid[i][j].railMask;
            if (m == 0) continue;

            glm::vec3 basePos = CellToWorld(i, j);
            glm::vec3 color = normalColor;
            float height = 0.02f;
            if (grid[i][j].railType == RailVisualType::Bridge) {
                height = 0.06f;
                color = bridgeColor;
            }
            else if (grid[i][j].railType == RailVisualType::Tunnel) {
                height = -0.02f;
                color = tunnelColor;
            }

            float o = CELL_SIZE * 0.25f;
            if (m & UP) RenderRailSegment(basePos, glm::vec3(0, 0, -o), height, RADIANS(90), color);
            if (m & DOWN) RenderRailSegment(basePos, glm::vec3(0, 0, +o), height, RADIANS(90), color);
            if (m & LEFT) RenderRailSegment(basePos, glm::vec3(-o, 0, 0), height, 0.0f, color);
            if (m & RIGHT) RenderRailSegment(basePos, glm::vec3(+o, 0, 0), height, 0.0f, color);
        }
    }
}

/* =========================================================
 *  Rendering helpers
 * ========================================================= */
void TrainGame::RenderMeshColor(Mesh* mesh, const glm::mat4& modelMatrix, const glm::vec3& color, float station_fullness)
{
    Shader* shader = shaders["TrainShader"];
    if (!mesh || !shader || !shader->program) return;
    shader->Use();
    glUniformMatrix4fv(shader->loc_model_matrix, 1, GL_FALSE, glm::value_ptr(modelMatrix));
    glUniformMatrix4fv(shader->loc_view_matrix, 1, GL_FALSE, glm::value_ptr(camera->GetViewMatrix()));
    glUniformMatrix4fv(shader->loc_projection_matrix, 1, GL_FALSE, glm::value_ptr(projectionMatrix));

    int locColor = glGetUniformLocation(shader->program, "object_color");
    if (locColor >= 0) glUniform3fv(locColor, 1, glm::value_ptr(color));

    int locFullness = glGetUniformLocation(shader->program, "station_fullness");
    if (locFullness >= 0) glUniform1f(locFullness, station_fullness);

    mesh->Render();
}

void TrainGame::RenderRailSegment(
    const glm::vec3& basePos,
    const glm::vec3& offset,
    float height,
    float rotY,
    const glm::vec3& color)
{
    glm::mat4 mtx(1);
    mtx = glm::translate(mtx, basePos + offset + glm::vec3(0, height, 0));
    mtx = glm::rotate(mtx, rotY, glm::vec3(0, 1, 0));
    mtx = glm::scale(mtx, glm::vec3(CELL_SIZE * 0.5f, 0.03f, CELL_SIZE * 0.15f));
    RenderMeshColor(meshes["box"], mtx, color);
}

void TrainGame::DrawBoxPart(const glm::vec3& basePos, float yaw,
    const glm::vec3& offset,
    const glm::vec3& scale,
    const glm::vec3& color)
{
    glm::mat4 m(1);
    m = glm::translate(m, basePos);
    m = glm::rotate(m, yaw, glm::vec3(0, 1, 0));
    m = glm::translate(m, offset);
    m = glm::scale(m, scale);
    RenderMeshColor(meshes["box"], m, color);
}

void TrainGame::DrawSpherePart(const glm::vec3& basePos, float yaw,
    const glm::vec3& offset,
    float radius,
    const glm::vec3& color)
{
    glm::mat4 m(1);
    m = glm::translate(m, basePos);
    m = glm::rotate(m, yaw, glm::vec3(0, 1, 0));
    m = glm::translate(m, offset);
    m = glm::scale(m, glm::vec3(radius));
    RenderMeshColor(meshes["sphere"], m, color);
}

void TrainGame::DrawCylinderPart(const glm::vec3& basePos, float yaw,
    const glm::vec3& offset,
    const glm::vec3& scale,
    const glm::vec3& color,
    float localRotX,
    float localRotY,
    float localRotZ)
{
    glm::mat4 m(1);
    m = glm::translate(m, basePos);
    m = glm::rotate(m, yaw, glm::vec3(0, 1, 0));
    m = glm::translate(m, offset);
    if (localRotX != 0) m = glm::rotate(m, localRotX, glm::vec3(1, 0, 0));
    if (localRotY != 0) m = glm::rotate(m, localRotY, glm::vec3(0, 1, 0));
    if (localRotZ != 0) m = glm::rotate(m, localRotZ, glm::vec3(0, 0, 1));
    m = glm::scale(m, scale);
    RenderMeshColor(meshes["cylinder"], m, color);
}

/* =========================================================
 *  Station / Rail / Train rendering
 * ========================================================= */
void TrainGame::RenderStation(const Station& s)
{
    glm::vec3 color;
    if (s.shape == StationShape::Circle) color = glm::vec3(0.2f, 0.7f, 0.95f);
    else if (s.shape == StationShape::Square) color = glm::vec3(0.95f, 0.65f, 0.2f);
    else color = glm::vec3(0.3f, 0.95f, 0.25f);

    glm::mat4 m(1);
    m = glm::translate(m, s.pos + glm::vec3(0, 0.25f, 0));
    m = glm::scale(m, glm::vec3(0.5f));

    float fullness = std::min((float)s.waitingPassengers.size() / 10.0f, 1.0f);

    if (s.shape == StationShape::Circle) RenderMeshColor(meshes["sphere"], m, color, fullness);
    else if (s.shape == StationShape::Pyramid) RenderMeshColor(meshes["pyramid"], m, color, fullness);
    else RenderMeshColor(meshes["box"], m, color, fullness);
}

void TrainGame::RenderLocomotive(const glm::vec3& pos, const glm::vec3& dir)
{
    float yaw = atan2(-dir.z, dir.x);
    glm::vec3 basePos = pos + glm::vec3(0, TRAIN_Y_OFFSET, 0);

    DrawBoxPart(basePos, yaw, glm::vec3(-0.12f, 0.125f, 0), glm::vec3(1.2f, 0.05f, 0.4f), glm::vec3(1, 1, 0));
    DrawBoxPart(basePos, yaw, glm::vec3(-0.5f, 0.35f, 0), glm::vec3(0.45f, 0.4f, 0.4f), glm::vec3(0, 1, 0));
    DrawCylinderPart(basePos, yaw, glm::vec3(0.075f, 0.27f, 0), glm::vec3(0.25f, 0.25f, 0.70f), glm::vec3(0,0,1), 0, RADIANS(90), 0);
    DrawCylinderPart(basePos, yaw, glm::vec3(0.45f, 0.27f, 0), glm::vec3(0.1f, 0.1f, 0.05f), glm::vec3(1, 0, 1), 0, RADIANS(90), 0);

    float xStart = -0.58f;
    for (int i = 0; i < 7; i++) {
        DrawCylinderPart(basePos, yaw, glm::vec3(xStart + i * 0.15f, 0.03f, 0.15f), glm::vec3(0.15f, 0.15f, 0.05f), glm::vec3(1, 0, 0), 0, 0, 0);
        DrawCylinderPart(basePos, yaw, glm::vec3(xStart + i * 0.15f, 0.03f, -0.15f), glm::vec3(0.15f, 0.15f, 0.05f), glm::vec3(1, 0, 0), 0, 0, 0);
    }
}

void TrainGame::RenderWagon(const glm::vec3& pos, const glm::vec3& dir)
{
    float yaw = atan2(-dir.z, dir.x);
    glm::vec3 basePos = pos + glm::vec3(0, TRAIN_Y_OFFSET, 0);

    DrawBoxPart(basePos, yaw, glm::vec3(0.0f, 0.125f, 0), glm::vec3(1.0f, 0.05f, 0.4f), glm::vec3(1, 1, 0 ));
    DrawBoxPart(basePos,yaw, glm::vec3(0.0f, 0.325f, 0), glm::vec3(1.0f, 0.45f, 0.4f), glm::vec3(0, 1.0f, 0));

    float xStart = -0.4f;
    for (int i = 0; i < 2; i++) {
        DrawCylinderPart(basePos, yaw, glm::vec3(xStart + i * 0.8f, 0.03f,  0.15f), glm::vec3(0.15f, 0.15f, 0.05f), glm::vec3(1, 0, 0), 0, 0, 0);
        DrawCylinderPart(basePos, yaw, glm::vec3(xStart + i * 0.8f, 0.03f, -0.15f), glm::vec3(0.15f, 0.15f, 0.05f), glm::vec3(1, 0, 0), 0, 0, 0);
    }
}

void TrainGame::RenderStationPassengers(const Station& s)
{
    float spacing = 0.15f;
    glm::vec3 basePos = s.pos + glm::vec3(-0.3f, 0.6f, -0.08f);

    for (int i = 0; i < (int)s.waitingPassengers.size(); i++)
    {
        glm::vec3 p = basePos + glm::vec3((i % 5) * spacing, 0, (i / 5) * spacing);
        RenderPassenger(p, s.waitingPassengers[i]);
    }
}

void TrainGame::RenderPassenger(const glm::vec3& pos, const Passenger& p)
{
    glm::mat4 m(1);
    m = glm::translate(m, pos);
    m = glm::scale(m, glm::vec3(0.12f));

    if (p.type == StationShape::Circle) RenderMeshColor(meshes["sphere"], m, { 0.2f,0.7f,1 });
    else if (p.type == StationShape::Square) RenderMeshColor(meshes["box"], m, { 1,0.7f,0.2f });
    else RenderMeshColor(meshes["pyramid"], m, { 0.3f,1,0.3f });
}

void TrainGame::RenderWagonPassengers(
    const GridTrain& t,
    int wagonIndex,
    const glm::vec3& wagonPos,
    const glm::vec3& wagonDir)
{
    glm::vec3 forward = glm::normalize(wagonDir);
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));

    float startHeight = 0.7f;
    float spacingX = 0.18f;
    float spacingZ = 0.18f;

    int startPassenger = wagonIndex * 6;
    int endPassenger = std::min(startPassenger + 6, (int)t.passengers.size());

    for (int i = startPassenger; i < endPassenger; i++)
    {
        int local = i - startPassenger;

        int row = local / 3;
        int col = local % 3;

        float xOffset = (col - 1) * spacingX;
        float zOffset = (row - 0.5f) * spacingZ;

        glm::vec3 pos =
            wagonPos +
            forward * xOffset -
            right * zOffset +
            glm::vec3(0, startHeight, 0);

        RenderPassenger(pos, t.passengers[i]);
    }
}

/* =========================================================
 *  Screen To World
 * ========================================================= */
glm::vec3 TrainGame::ScreenToWorldOnGround(int mouseX, int mouseY)
{
    glm::ivec2 res = window->GetResolution();
    float x = 2.f * mouseX / res.x - 1.f;
    float y = 1.f - 2.f * mouseY / res.y;

    glm::vec4 clip(x, y, -1, 1);
    glm::vec4 eye = glm::inverse(projectionMatrix) * clip;
    eye.z = -1;
    eye.w = 0;

    glm::vec3 dir = glm::normalize(glm::vec3(glm::inverse(camera->GetViewMatrix()) * eye));
    glm::vec3 origin = camera->position;

    if (fabs(dir.y) < 1e-4f) return origin;

    float t = -origin.y / dir.y;
    return origin + dir * t;
}

/* =========================================================
 *  Station 
 * ========================================================= */
bool TrainGame::IsValidStationCell(int i, int j) const
{
    if (grid[i][j].type != CellType::Grass) return false;
    if (grid[i][j].hasStation) return false;
    if (i == 0 || i == GRID_W - 1) return false;
    if (j == 0 || j == GRID_H - 1) return false;

    glm::vec3 pos = CellToWorld(i, j);
    for (auto& s : stations)
        if (glm::distance(s.pos, pos) < CELL_SIZE * 2.0f)
            return false;

    return true;
}

bool TrainGame::SpawnRandomStation()
{
    const int MAX_TRIES = 100;

    for (int t = 0; t < MAX_TRIES; t++) {
        int i = rand() % GRID_H;
        int j = rand() % GRID_W;

        if (!IsValidStationCell(i, j))
            continue;

        glm::vec3 pos = CellToWorld(i, j);

        StationShape shape;
        int shapeTypeChance = rand() % 3;
        if (shapeTypeChance == 0) shape = StationShape::Square;
        else if (shapeTypeChance == 1) shape = StationShape::Circle;
        else shape = StationShape::Pyramid;

        AddStation(pos, shape);
        grid[i][j].hasStation = true;

        return true;
    }

    return false;
}

void TrainGame::AddStation(const glm::vec3& pos, StationShape shape)
{
    Station s;
    s.id = (int)stations.size();
    s.pos = pos;
    s.shape = shape;
    stations.push_back(s);
}


int TrainGame::PickStationAt(const glm::vec3& p) const
{
    for (auto& s : stations)
        if (glm::distance(s.pos, p) < pickRadius)
            return s.id;
    return -1;
}

int TrainGame::GetStationAtCell(int i, int j)
{
    for (auto& s : stations)
    {
        int si, sj;
        WorldToCell(s.pos, si, sj);
        if (si == i && sj == j)
            return s.id;
    }
    return -1;
}

/* =========================================================
 *  Train Spawning and Updating
 * ========================================================= */
int TrainGame::SpawnTrainAtCell(int i, int j)
{
    for (auto& t : gridTrains)
        if (t.i == i && t.j == j)
            return -1;

    GridTrain t;
    t.i = i;
    t.j = j;
    t.progress = 0.0f;
    t.dir = 0;

    glm::vec3 pos = GetTrainPos(t);
    for (int k = 0; k < 20; k++)
        t.trail.push_back(pos);

    unsigned char m = grid[i][j].railMask;
    if (m & UP) t.dir = 0;
    else if (m & RIGHT) t.dir = 1;
    else if (m & DOWN) t.dir = 2;
    else if (m & LEFT) t.dir = 3;

    gridTrains.push_back(t);

    return gridTrains.size() - 1;
}

void TrainGame::UpdateGridTrains(float dt)
{
    float speed = 2.0f;

    for (auto& t : gridTrains)
    {
        if (t.stopping)
        {
            t.stopTimer += dt;

            if (t.stopTimer >= 0.5f)
            {
                t.stopTimer = 0.0f;
                ProcessStationPassengers(t);
            }

            continue;
        }

        t.progress += dt * speed;

        while (t.progress >= 1.0f)
        {
            t.progress -= 1.0f;

            int ni = t.i + di[t.dir];
            int nj = t.j + dj[t.dir];

            if (ni < 0 || nj < 0 || ni >= GRID_H || nj >= GRID_W ||
                grid[ni][nj].railMask == 0)
            {
                t.dir = OppositeDir(t.dir);
                ResetTrainTrail(t);
                break;
            }

            t.i = ni;
            t.j = nj;

            int stationId = GetStationAtCell(t.i, t.j);
            if (stationId >= 0)
            {
                bool hasPassengersToDrop = false;
                bool canPickUpPassengers = false;

                for (const auto& passenger : t.passengers)
                {
                    if (passenger.type == stations[stationId].shape)
                    {
                        hasPassengersToDrop = true;
                        break;
                    }
                }

                if (t.passengers.size() < TrainCapacity(t) &&
                    !stations[stationId].waitingPassengers.empty())
                {
                    canPickUpPassengers = true;
                }

                if (hasPassengersToDrop || canPickUpPassengers)
                {
                    StartStationStop(t, stationId);
                    break;
                }
                else
                {
                    t.dir = ChooseNextDirection(t.i, t.j, t.dir);
                    continue;
                }
            }

            t.dir = ChooseNextDirection(t.i, t.j, t.dir);
        }

        UpdateTrainTrail(t);
    }
}

void TrainGame::ResetTrainTrail(GridTrain& t)
{
    t.trail.clear();
    glm::vec3 pos = GetTrainPos(t);
    for (int k = 0; k < 30; k++)
        t.trail.push_back(pos);
}

void TrainGame::UpdateTrainTrail(GridTrain& t)
{
    glm::vec3 pos = GetTrainPos(t);
    t.trail.push_front(pos);
    if (t.trail.size() > 500) t.trail.pop_back();
}

int TrainGame::ChooseNextDirection(int i, int j, int currentDir)
{
    unsigned char mask = grid[i][j].railMask;
    int backDir = OppositeDir(currentDir);

    if (mask & DirToMask(currentDir))
        return currentDir;

    for (int d = 0; d < 4; d++) {
        if (d == backDir) continue;
        if (mask & DirToMask(d))
            return d;
    }

    return backDir;
}

void TrainGame::ProcessStationPassengers(GridTrain& train)
{
    Station& station = stations[train.stationId];

    // Descarcare
    if (train.unloadIndex < (int)train.passengers.size())
    {
        Passenger& p = train.passengers[train.unloadIndex];

        bool delivered =
            (station.shape == StationShape::Circle && p.type == StationShape::Circle) ||
            (station.shape == StationShape::Square && p.type == StationShape::Square) ||
            (station.shape == StationShape::Pyramid && p.type == StationShape::Pyramid);

        if (delivered) {
            totalDeliveredPassengers++;
            currentPoints++;
            train.passengers.erase(train.passengers.begin() + train.unloadIndex);
        }
        else train.unloadIndex++;

        return;
    }
    // end

    // Incarcare
    if (!station.waitingPassengers.empty() &&
        (int)train.passengers.size() < TrainCapacity(train))
    {
        train.passengers.push_back(station.waitingPassengers.back());
        station.waitingPassengers.pop_back();
        return;
    }
    // end

    // Plecare
    train.stopping = false;
    train.stationId = -1;
    train.unloadIndex = 0;
    train.dir = ChooseNextDirection(train.i, train.j, train.dir);
    // end
}

void TrainGame::StartStationStop(GridTrain& train, int stationId)
{
    train.stopping = true;
    train.stationId = stationId;
    train.stopTimer = 0.0f;
    train.unloadIndex = 0;
}

/* =========================================================
 *  Train Positioning, Direction and Misc. Info
 * ========================================================= */
glm::vec3 TrainGame::GetTrainPos(const GridTrain& t)
{
    glm::vec3 p1 = CellToWorld(t.i, t.j);

    int ni = t.i + di[t.dir];
    int nj = t.j + dj[t.dir];

    glm::vec3 p2 = p1;
    if (ni >= 0 && nj >= 0 && ni < GRID_H && nj < GRID_W) p2 = CellToWorld(ni, nj);

    return glm::mix(p1, p2, t.progress) + glm::vec3(0, TRAIN_Y_OFFSET, 0);
}

glm::vec3 TrainGame::GetTrainDir(const GridTrain& t)
{
    glm::vec3 p1 = CellToWorld(t.i, t.j);

    int ni = t.i + di[t.dir];
    int nj = t.j + dj[t.dir];

    glm::vec3 p2 = p1;
    if (ni >= 0 && nj >= 0 && ni < GRID_H && nj < GRID_W) p2 = CellToWorld(ni, nj);

    glm::vec3 d = p2 - p1;
    d.y = 0;
    if (glm::length(d) < 0.001f) return glm::vec3(1, 0, 0);

    return glm::normalize(d);
}

int TrainGame::PickGridTrainAt(const glm::vec3& p)
{
    for (int i = 0; i < (int)gridTrains.size(); i++)
    {
        glm::vec3 tp = CellToWorld(gridTrains[i].i, gridTrains[i].j);
        if (glm::distance(tp, p) < CELL_SIZE * 0.4f) return i;
    }
    return -1;
}

int TrainGame::TrainCapacity(const GridTrain& t)
{
    return t.wagons * 6;
}

/* =========================================================
 *  Rail Construction and Deletion
 * ========================================================= */
void TrainGame::HandleStationConnection(int ci, int cj, const glm::vec3& hit)
{
    int stationId = PickStationAt(hit);
    if (stationId < 0) return;

    if (selectedStation < 0) {
        selectedStation = stationId;
        return;
    }

    if (selectedStation == stationId) {
        selectedStation = -1;
        return;
    }

    BuildRailPath(selectedStation, stationId);
    selectedStation = -1;
}

void TrainGame::BuildRailPath(int startStationId, int endStationId)
{
    int si, sj, ei, ej;
    if (!WorldToCell(stations[startStationId].pos, si, sj) ||
        !WorldToCell(stations[endStationId].pos, ei, ej))
    {
        return;
    }

    std::vector<std::pair<int, int>> path;
    if (!FindPathBFS(si, sj, ei, ej, path))
    {
        return;
    }

    auto tempMask = grid;
    for (int k = 0; k + 1 < (int)path.size(); k++)
    {
        int i1 = path[k].first;
        int j1 = path[k].second;
        int i2 = path[k + 1].first;
        int j2 = path[k + 1].second;

        unsigned char m1 = 0, m2 = 0;

        if (i2 == i1 && j2 == j1 + 1) { m1 = RIGHT; m2 = LEFT; }
        else if (i2 == i1 && j2 == j1 - 1) { m1 = LEFT; m2 = RIGHT; }
        else if (i2 == i1 + 1 && j2 == j1) { m1 = DOWN; m2 = UP; }
        else if (i2 == i1 - 1 && j2 == j1) { m1 = UP; m2 = DOWN; }

        unsigned char newMask1 = tempMask[i1][j1].railMask | m1;
        unsigned char newMask2 = tempMask[i2][j2].railMask | m2;

        if (CountBits(newMask1) == 3 || CountBits(newMask2) == 3)
        {
            return;
        }

        tempMask[i1][j1].railMask = newMask1;
        tempMask[i2][j2].railMask = newMask2;
    }

    for (int k = 0; k + 1 < (int)path.size(); k++)
    {
        int i1 = path[k].first;
        int j1 = path[k].second;
        int i2 = path[k + 1].first;
        int j2 = path[k + 1].second;

        unsigned char m1 = 0, m2 = 0;

        if (i2 == i1 && j2 == j1 + 1) { m1 = RIGHT; m2 = LEFT; }
        else if (i2 == i1 && j2 == j1 - 1) { m1 = LEFT; m2 = RIGHT; }
        else if (i2 == i1 + 1 && j2 == j1) { m1 = DOWN; m2 = UP; }
        else if (i2 == i1 - 1 && j2 == j1) { m1 = UP; m2 = DOWN; }

        grid[i1][j1].railMask |= m1;
        grid[i2][j2].railMask |= m2;

        if (grid[i2][j2].type == CellType::Water)
            grid[i2][j2].railType = RailVisualType::Bridge;
        else if (grid[i2][j2].type == CellType::Mountain)
            grid[i2][j2].railType = RailVisualType::Tunnel;
    }
}

void TrainGame::EraseRailFromCell(int si, int sj)
{
    if (grid[si][sj].railMask == 0)  return;

    unsigned char m = grid[si][sj].railMask;
    for (int d = 0; d < 4; d++)
        if (m & DirToMask(d))
            EraseRailsInDirection(si, sj, d);
}

void TrainGame::EraseRailsInDirection(int si, int sj, int dir)
{
    int i = si;
    int j = sj;
    int curDir = dir;
    while (true)
    {
        if (grid[i][j].hasStation) break;

        unsigned char mask = grid[i][j].railMask;

        grid[i][j].railMask &= ~DirToMask(curDir);

        int ni = i + di[curDir];
        int nj = j + dj[curDir];
        if (ni < 0 || nj < 0 || ni >= GRID_H || nj >= GRID_W) break;

        grid[ni][nj].railMask &= ~DirToMask(OppositeDir(curDir));

        i = ni;
        j = nj;
        if (grid[i][j].railMask == 0) break;

        unsigned char newMask = grid[i][j].railMask;
        bool found = false;
        for (int d = 0; d < 4; d++)
        {
            if (newMask & DirToMask(d))
            {
                curDir = d;
                found = true;
                break;
            }
        }

        if (!found) break;
    }
}

void TrainGame::RemoveTrainsOnBrokenRails()
{
    for (int k = (int)gridTrains.size() - 1; k >= 0; k--)
    {
        auto& t = gridTrains[k];
        if (grid[t.i][t.j].railMask == 0) {
            currentPoints += 10 + t.wagons * 5;
            gridTrains.erase(gridTrains.begin() + k);
        }
    }
}

bool TrainGame::HasRailAt(int i, int j)
{
    return grid[i][j].railMask != 0;
}

/* =========================================================
 *  Game Restart
 * ========================================================= */
void TrainGame::RestartGame()
{
    int di[4] = { -1, 0, 1, 0 };
    int dj[4] = { 0, 1, 0, -1 };

    grid.clear();
    gridTrains.clear();
    stations.clear();
    stationFullnessTimers.clear();
    selectedStation = -1;
    gameOver = circleExists = squareExists = pyramidExists = false;
    totalDeliveredPassengers = 0;
    currentPoints = 15;
    gameTime = 0;

    InitGrid();

    SpawnRandomStation();
    SpawnRandomStation();
}

/* =========================================================
 *  BFS
 * ========================================================= */
bool TrainGame::FindPathBFS(int si, int sj, int ti, int tj, std::vector<std::pair<int, int>>& outPath)
{
    std::vector<std::vector<bool>> visited(GRID_H, std::vector<bool>(GRID_W, false));
    std::vector<std::vector<std::pair<int, int>>> parent(GRID_H, std::vector<std::pair<int, int>>(GRID_W, { -1, -1 }));
    std::queue<std::pair<int, int>> q;

    visited[si][sj] = true;
    q.push({ si, sj });

    const int di[4] = { -1, 1, 0, 0 };
    const int dj[4] = { 0, 0, -1, 1 };

    bool found = false;
    while (!q.empty()) {
        std::pair<int, int> cur = q.front();
        q.pop();

        int ci = cur.first;
        int cj = cur.second;

        if (ci == ti && cj == tj) {
            found = true;
            break;
        }

        for (int d = 0; d < 4; d++) {
            int ni = ci + di[d];
            int nj = cj + dj[d];

            if (ni < 0 || nj < 0 || ni >= GRID_H || nj >= GRID_W) continue;

            if (visited[ni][nj]) continue;

            visited[ni][nj] = true;
            parent[ni][nj] = { ci, cj };
            q.push({ ni, nj });
        }
    }

    if (!found) return false;

    outPath.clear();
    int ci = ti, cj = tj;
    while (!(ci == si && cj == sj)) {
        outPath.push_back({ ci, cj });
        auto p = parent[ci][cj];
        ci = p.first;
        cj = p.second;
    }
    outPath.push_back({ si, sj });

    std::reverse(outPath.begin(), outPath.end());
    return true;
}

/* =========================================================
 *  Input callbacks
 * ========================================================= */
void TrainGame::OnInputUpdate(float dt, int)
{
    if (window->MouseHold(GLFW_MOUSE_BUTTON_RIGHT))
    {
        if (window->KeyHold(GLFW_KEY_W)) camera->TranslateForward(dt * cameraSpeed);
        if (window->KeyHold(GLFW_KEY_S)) camera->TranslateForward(-dt * cameraSpeed);
        if (window->KeyHold(GLFW_KEY_A)) camera->TranslateRight(-dt * cameraSpeed);
        if (window->KeyHold(GLFW_KEY_D)) camera->TranslateRight(dt * cameraSpeed);
        if (window->KeyHold(GLFW_KEY_Q)) camera->TranslateUpward(-dt * cameraSpeed);
        if (window->KeyHold(GLFW_KEY_E)) camera->TranslateUpward(dt * cameraSpeed);
    }
}

void TrainGame::OnKeyPress(int key, int)
{
    if (key == GLFW_KEY_P)
        projectionMatrix = glm::perspective(fov, aspect, zNear, zFar);
    if (key == GLFW_KEY_O)
        projectionMatrix = glm::ortho(left, right, bottom, top, zNear, zFar);
    if (key == GLFW_KEY_SPACE)
        RestartGame();
}

void TrainGame::OnKeyRelease(int, int) {}

void TrainGame::OnMouseMove(int, int, int dx, int dy)
{
    if (window->MouseHold(GLFW_MOUSE_BUTTON_RIGHT))
    {
        camera->RotateFirstPerson_OY(-dx * sensivityOX);
        camera->RotateFirstPerson_OX(-dy * sensivityOY);
    }
}

void TrainGame::OnMouseBtnPress(int mx, int my, int button, int)
{
    glm::vec3 hit = ScreenToWorldOnGround(mx, my);
    int ci, cj;
    if (!WorldToCell(hit, ci, cj)) return;

    if (button == 1) {
        int trainId = PickGridTrainAt(hit);
        if (trainId >= 0) {
            if (gridTrains[trainId].wagons < 5 && currentPoints >= 5) {
                gridTrains[trainId].wagons++;
                currentPoints -= 5;
            }
        }
        else {
            HandleStationConnection(ci, cj, hit);
        }
    }
    else if (button == 2) {
        if (HasRailAt(ci, cj) && currentPoints >= 10) {
            int trainId = SpawnTrainAtCell(ci, cj);
            if (trainId != -1) gridTrains[trainId].wagons++;
            currentPoints -= 15;
        }
    }
    else if (button == 4) {
        if (HasRailAt(ci, cj)) {
            EraseRailFromCell(ci, cj);
            RemoveTrainsOnBrokenRails();
        }
    }
}

void TrainGame::OnMouseBtnRelease(int, int, int, int) {}
void TrainGame::OnMouseScroll(int, int, int, int) {}
void TrainGame::OnWindowResize(int, int) {}
