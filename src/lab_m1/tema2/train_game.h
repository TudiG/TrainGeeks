#pragma once

#include <queue>
#include <limits>

#include "components/simple_scene.h"
#include "include/lab_camera.h"
#include "components/text_renderer.h"

namespace m1
{
    class TrainGame : public gfxc::SimpleScene
    {
    public:
        TrainGame();
        ~TrainGame();

        void Init() override;

    private:
        void FrameStart() override;
        void Update(float deltaTimeSeconds) override;
        void FrameEnd() override;

        void OnInputUpdate(float deltaTime, int mods) override;
        void OnKeyPress(int key, int mods) override;
        void OnKeyRelease(int key, int mods) override;
        void OnMouseMove(int mouseX, int mouseY, int deltaX, int deltaY) override;
        void OnMouseBtnPress(int mouseX, int mouseY, int button, int mods) override;
        void OnMouseBtnRelease(int mouseX, int mouseY, int button, int mods) override;
        void OnMouseScroll(int mouseX, int mouseY, int offsetX, int offsetY) override;
        void OnWindowResize(int width, int height) override;

    protected:
        implemented::Camera* camera;
        glm::mat4 projectionMatrix;
        bool renderCameraTarget;

        bool isPerspective;
        float fov, aspect, zNear, zFar;
        float left, right, bottom, top;
        float cameraSpeed;
        float sensivityOX, sensivityOY;

        // ===== GRID =====
        static constexpr int GRID_W = 16;
        static constexpr int GRID_H = 16;
        static constexpr float CELL_SIZE = 1.0f;
        float mapHalfSize = GRID_W * CELL_SIZE * 0.5f;

        enum class CellType {
            Grass,
            Water,
            Mountain
        };

        enum class RailVisualType {
            Normal,
            Bridge,
            Tunnel
        };

        enum RailDir {
            NONE = 0,
            UP = 1 << 0,
            DOWN = 1 << 1,
            LEFT = 1 << 2,
            RIGHT = 1 << 3
        };

        struct Cell {
            CellType type = CellType::Grass;
            unsigned char railMask = 0;
            RailVisualType railType = RailVisualType::Normal;
            bool hasStation = false;
        };

        std::vector<std::vector<Cell>> grid;

        void InitGrid();
        glm::vec3 CellToWorld(int i, int j) const;
        bool WorldToCell(const glm::vec3& p, int& i, int& j) const;

        // ===== GAME DATA =====
        enum class StationShape { 
            Circle,
            Square,
            Pyramid
        };

        struct Passenger {
            StationShape type;
        };

        struct Station {
            int id;
            glm::vec3 pos;
            StationShape shape;

            std::vector<Passenger> waitingPassengers;
        };
        std::vector<Station> stations;

        struct GridTrain
        {
            int i, j;
            int dir;
            float progress;
            int wagons = 0;
            std::deque<glm::vec3> trail;
            std::vector<Passenger> passengers;

            bool stopping = false;
            int stationId = -1;
            float stopTimer = 0.0f;
            int unloadIndex = 0;
        };
        std::vector<GridTrain> gridTrains;

        float stationMaxFullnessTimer = 30.0f; // 30.0f
        std::vector<float> stationFullnessTimers;

        bool gameOver, circleExists, squareExists, pyramidExists;
        int selectedStation;
        float pickRadius = 0.75f;
        int di[4] = { -1, 0, 1, 0 };
        int dj[4] = { 0, 1, 0, -1 };
        float TRAIN_Y_OFFSET = 0.07f;
        float locomotiveLength = 1.35f;
        float wagonSpacing = 1.15f;
        float stationSpawnTimer = 0.0f;

        float stationSpawnInterval = 30.0f; // 30.0f
        float passengerSpawnInterval = 8.0f; // 8.0f

        float gameTime;
        int totalDeliveredPassengers;
        int currentPoints;

        gfxc::TextRenderer* textRenderer;

        // ===== HELPERS AND FUNCTIONS =====
        void RenderRailSegment(
            const glm::vec3& basePos,
            const glm::vec3& offset,
            float height,
            float rotY,
            const glm::vec3& color);

        void DrawBoxPart(const glm::vec3& basePos, float yaw,
            const glm::vec3& offset,
            const glm::vec3& scale,
            const glm::vec3& color);

        void DrawSpherePart(const glm::vec3& basePos, float yaw,
            const glm::vec3& offset,
            float radius,
            const glm::vec3& color);

        void DrawCylinderPart(const glm::vec3& basePos, float yaw,
            const glm::vec3& offset,
            const glm::vec3& scale,
            const glm::vec3& color,
            float localRotX,
            float localRotY,
            float localRotZ);

        glm::vec3 ScreenToWorldOnGround(int mouseX, int mouseY);
        void AddStation(const glm::vec3& pos, StationShape shape);
        int PickStationAt(const glm::vec3& p) const;
        void RenderStation(const Station& s);
        void RenderLocomotive(const glm::vec3& pos, const glm::vec3& dir);
        void RenderWagon(const glm::vec3& pos, const glm::vec3& dir);
        void RenderMeshColor(Mesh* mesh, const glm::mat4& modelMatrix, const glm::vec3& color, float station_fullness = 0.0f);
        void RenderGrid();
        void RenderGridRails();
        bool IsValidStationCell(int i, int j) const;
        bool SpawnRandomStation();
        bool FindPathBFS(int si, int sj, int ti, int tj, std::vector<std::pair<int, int>>& outPath);
        bool HasRailAt(int i, int j);
        int SpawnTrainAtCell(int i, int j);
        int ChooseNextDirection(int i, int j, int comingFromDir);
        void UpdateGridTrains(float dt);
        void EraseRailsInDirection(int si, int sj, int dir);
        void EraseRailFromCell(int si, int sj);
        void RemoveTrainsOnBrokenRails();
        int PickGridTrainAt(const glm::vec3& p);
        glm::vec3 GetTrainPos(const GridTrain &t);
        glm::vec3 GetTrainDir(const GridTrain &t);
        int TrainCapacity(const GridTrain& t);
        int GetStationAtCell(int i, int j);
        void RenderStationPassengers(const Station& s);
        void RenderWagonPassengers( const GridTrain& t, int wagonIndex, const glm::vec3& wagonPos, const glm::vec3& wagonDir);
        void RenderPassenger(const glm::vec3& pos, const Passenger& p);
        void StartStationStop(GridTrain& train, int stationId);
        void ProcessStationPassengers(GridTrain& train);
        void ResetTrainTrail(GridTrain& t);
        void UpdateTrainTrail(GridTrain& t);
        void BuildRailPath(int startStationId, int endStationId);
        void HandleStationConnection(int ci, int cj, const glm::vec3& hit);
        void RestartGame();

        bool AreOppositeDirs(int dir1, int dir2)
        {
            return (dir1 == 0 && dir2 == 2) || (dir1 == 2 && dir2 == 0) ||
                (dir1 == 1 && dir2 == 3) || (dir1 == 3 && dir2 == 1);
        }

        int OppositeDir(int d)
        {
            return (d + 2) % 4;
        }

        int DirToMask(int d)
        {
            if (d == 0) return UP;
            if (d == 1) return RIGHT;
            if (d == 2) return DOWN;
            return LEFT;
        }

        int TrainGame::CountBits(unsigned char mask)
        {
            int count = 0;
            for (int i = 0; i < 4; i++) {
                if (mask & (1 << i)) count++;
            }
            return count;
        }

    };
}
