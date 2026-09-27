#pragma once

#include "ProfilerTask.h"
#include "imgui.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <map>
#include <vector>

namespace ImGuiUtils
{
  class ProfilerGraph
  {
  public:
    int frameWidth;
    int frameSpacing;
    bool useColoredLegendText;

    ProfilerGraph(size_t framesCount)
    {
      frames.resize(framesCount);
      for (auto &frame : frames)
        frame.tasks.reserve(100);
      frameWidth = 3;
      frameSpacing = 1;
      useColoredLegendText = false;
    }

    void LoadFrameData(const legit::ProfilerTask *tasks, size_t count)
    {
      auto &currFrame = frames[currFrameIndex];
      currFrame.tasks.resize(0);
      currFrame.totalTime = 0.0f;
      for (size_t taskIndex = 0; taskIndex < count; taskIndex++)
      {
        if (taskIndex == 0)
          currFrame.tasks.push_back(tasks[taskIndex]);
        else
        {
          if (tasks[taskIndex - 1].color != tasks[taskIndex].color || tasks[taskIndex - 1].name != tasks[taskIndex].name)
          {
            currFrame.tasks.push_back(tasks[taskIndex]);
          }
          else
          {
            currFrame.tasks.back().endTime = tasks[taskIndex].endTime;
          }
        }
        currFrame.totalTime += float(tasks[taskIndex].endTime - tasks[taskIndex].startTime);
      }
      currFrame.taskStatsIndex.resize(currFrame.tasks.size());

      for (size_t taskIndex = 0; taskIndex < currFrame.tasks.size(); taskIndex++)
      {
        auto &task = currFrame.tasks[taskIndex];
        auto it = taskNameToStatsIndex.find(task.name);
        if (it == taskNameToStatsIndex.end())
        {
          taskNameToStatsIndex[task.name] = taskStats.size();
          TaskStats taskStat;
          taskStat.maxTime = -1.0;
          taskStats.push_back(taskStat);
        }
        currFrame.taskStatsIndex[taskIndex] = taskNameToStatsIndex[task.name];
      }
      currFrameIndex = (currFrameIndex + 1) % frames.size();

      RebuildTaskStats(currFrameIndex, frames.size());
    }
    float GetTotalTaskTime(int frameIndexOffset)
    {
      return frames[GetCurrFrameIndex(size_t(frameIndexOffset))].totalTime;
    }
    void RenderTimings(int graphWidth, int legendWidth, int height, int frameIndexOffset, float maxFrameTime)
    {
      ImDrawList* drawList = ImGui::GetWindowDrawList();
      const ImVec2 widgetPos = ImGui::GetCursorScreenPos();
      RenderGraph(drawList, widgetPos, ImVec2(float(graphWidth), float(height)), size_t(frameIndexOffset), maxFrameTime);
      RenderLegend(drawList, widgetPos + ImVec2(float(graphWidth), 0.0f), ImVec2(float(legendWidth), float(height)), size_t(frameIndexOffset), maxFrameTime);
      ImGui::Dummy(ImVec2(float(graphWidth + legendWidth), float(height)));
    }

  private:
    size_t GetCurrFrameIndex(size_t frameIndexOffset)
    {
      return (currFrameIndex - frameIndexOffset - 1 + 2 * frames.size()) % frames.size();
    }
    void RebuildTaskStats(size_t endFrame, size_t framesCount)
    {
      for (auto &taskStat : taskStats)
      {
        taskStat.maxTime = -1.0;
        taskStat.priorityOrder = size_t(-1);
        taskStat.onScreenIndex = size_t(-1);
      }

      for (size_t frameNumber = 0; frameNumber < framesCount; frameNumber++)
      {
        size_t frameIndex = (endFrame - 1 - frameNumber + frames.size()) % frames.size();
        auto &frame = frames[frameIndex];
        for (size_t taskIndex = 0; taskIndex < frame.tasks.size(); taskIndex++)
        {
          auto &task = frame.tasks[taskIndex];
          auto &stats = taskStats[frame.taskStatsIndex[taskIndex]];
          stats.maxTime = std::max(stats.maxTime, task.endTime - task.startTime);
        }
      }
      std::vector<size_t> statPriorities;
      statPriorities.resize(taskStats.size());
      for(size_t statIndex = 0; statIndex < taskStats.size(); statIndex++)
        statPriorities[statIndex] = statIndex;

      std::sort(statPriorities.begin(), statPriorities.end(), [this](size_t left, size_t right) {return taskStats[left].maxTime > taskStats[right].maxTime; });
      for (size_t statNumber = 0; statNumber < taskStats.size(); statNumber++)
      {
        size_t statIndex = statPriorities[statNumber];
        taskStats[statIndex].priorityOrder = statNumber;
      }
    }
    void RenderGraph(ImDrawList *drawList, ImVec2 graphPos, ImVec2 graphSize, size_t frameIndexOffset, float maxFrameTime)
    {
      Rect(drawList, graphPos, graphPos + graphSize, 0xffffffff, false);
      float heightThreshold = 1.0f;

      for (size_t frameNumber = 0; frameNumber < frames.size(); frameNumber++)
      {
        size_t frameIndex = GetCurrFrameIndex(frameIndexOffset + frameNumber);

        ImVec2 framePos = graphPos + ImVec2(graphSize.x - 1.0f - float(frameWidth) - float(frameWidth + frameSpacing) * float(frameNumber), graphSize.y - 1.0f);
        if (framePos.x < graphPos.x + 1)
          break;
        ImVec2 taskPos = framePos;
        auto &frame = frames[frameIndex];
        for (const auto& task : frame.tasks)
        {
          float taskStartHeight = (float(task.startTime) / maxFrameTime) * graphSize.y;
          float taskEndHeight = (float(task.endTime) / maxFrameTime) * graphSize.y;
          if (std::abs(taskEndHeight - taskStartHeight) > heightThreshold)
            Rect(drawList, taskPos + ImVec2(0.0f, -taskStartHeight), taskPos + ImVec2(float(frameWidth), -taskEndHeight), task.color, true);
        }
      }
    }
    void RenderLegend(ImDrawList *drawList, ImVec2 legendPos, ImVec2 legendSize, size_t frameIndexOffset, float maxFrameTime)
    {
      float markerLeftRectMargin = 3.0f;
      float markerLeftRectWidth = 5.0f;
      float markerMidWidth = 30.0f;
      float markerRightRectWidth = 10.0f;
      float markerRigthRectMargin = 3.0f;
      float markerRightRectHeight = 10.0f;
      float markerRightRectSpacing = 4.0f;
      float nameOffset = ImGui::CalcTextSize("[00.00").x;
      ImVec2 textMargin = ImVec2(5.0f, -3.0f);

      auto &currFrame = frames[GetCurrFrameIndex(frameIndexOffset)];
      size_t maxTasksCount = size_t(legendSize.y / (markerRightRectHeight + markerRightRectSpacing));

      for (auto &taskStat : taskStats)
      {
        taskStat.onScreenIndex = size_t(-1);
      }

      size_t tasksToShow = std::min<size_t>(taskStats.size(), maxTasksCount);
      size_t tasksShownCount = 0;
      for (size_t taskIndex = 0; taskIndex < currFrame.tasks.size(); taskIndex++)
      {
        auto &task = currFrame.tasks[taskIndex];
        auto &stat = taskStats[currFrame.taskStatsIndex[taskIndex]];

        if (stat.priorityOrder >= tasksToShow)
          continue;

        if (stat.onScreenIndex == size_t(-1))
        {
          stat.onScreenIndex = tasksShownCount++;
        }
        else
          continue;
        float taskStartHeight = (float(task.startTime) / maxFrameTime) * legendSize.y;
        float taskEndHeight = (float(task.endTime) / maxFrameTime) * legendSize.y;

        ImVec2 markerLeftRectMin = legendPos + ImVec2(markerLeftRectMargin, legendSize.y);
        ImVec2 markerLeftRectMax = markerLeftRectMin + ImVec2(markerLeftRectWidth, 0.0f);
        markerLeftRectMin.y -= taskStartHeight;
        markerLeftRectMax.y -= taskEndHeight;

        ImVec2 markerRightRectMin = legendPos + ImVec2(markerLeftRectMargin + markerLeftRectWidth + markerMidWidth, legendSize.y - markerRigthRectMargin - (markerRightRectHeight + markerRightRectSpacing) * float(stat.onScreenIndex));
        ImVec2 markerRightRectMax = markerRightRectMin + ImVec2(markerRightRectWidth, -markerRightRectHeight);
        RenderTaskMarker(drawList, markerLeftRectMin, markerLeftRectMax, markerRightRectMin, markerRightRectMax, task.color);

        uint32_t textColor = useColoredLegendText ? task.color : legit::Colors::imguiText;

        float taskTimeMs = float(task.endTime - task.startTime) * 1000.0f;
        char timeText[32];
        std::snprintf(timeText, sizeof(timeText), "[%.2f", taskTimeMs);
        char nameText[128];
        std::snprintf(nameText, sizeof(nameText), "ms] %s", task.name.c_str());

        Text(drawList, markerRightRectMax + textMargin + ImVec2(nameOffset - ImGui::CalcTextSize(timeText).x, 0.0f), textColor, timeText);
        Text(drawList, markerRightRectMax + textMargin + ImVec2(nameOffset, 0.0f), textColor, nameText);
      }
    }

    static void Rect(ImDrawList *drawList, ImVec2 minPoint, ImVec2 maxPoint, uint32_t col, bool filled = true)
    {
      if(filled)
        drawList->AddRectFilled(minPoint, maxPoint, col);
      else
        drawList->AddRect(minPoint, maxPoint, col);
    }
    static void Text(ImDrawList *drawList, ImVec2 point, uint32_t col, const char *text)
    {
      drawList->AddText(point, col, text);
    }
    static void RenderTaskMarker(ImDrawList *drawList, ImVec2 leftMinPoint, ImVec2 leftMaxPoint, ImVec2 rightMinPoint, ImVec2 rightMaxPoint, uint32_t col)
    {
      Rect(drawList, leftMinPoint, leftMaxPoint, col, true);
      Rect(drawList, rightMinPoint, rightMaxPoint, col, true);
      std::array<ImVec2, 4> points = {
        ImVec2(leftMaxPoint.x, leftMinPoint.y),
        ImVec2(leftMaxPoint.x, leftMaxPoint.y),
        ImVec2(rightMinPoint.x, rightMaxPoint.y),
        ImVec2(rightMinPoint.x, rightMinPoint.y)
      };
      drawList->AddConvexPolyFilled(points.data(), int(points.size()), col);
    }
    struct FrameData
    {
      std::vector<legit::ProfilerTask> tasks;
      std::vector<size_t> taskStatsIndex;
      float totalTime;
    };

    struct TaskStats
    {
      double maxTime;
      size_t priorityOrder;
      size_t onScreenIndex;
    };
    std::vector<TaskStats> taskStats;
    std::map<std::string, size_t> taskNameToStatsIndex;

    std::vector<FrameData> frames;
    size_t currFrameIndex = 0;
  };

  class ProfilersWindow
  {
  public:
    ProfilersWindow(float maxFrameTime = 1.0f / 60.0f):
      cpuGraph(300),
      gpuGraph(300),
      maxFrameTime(maxFrameTime)
    {
      stopProfiling = false;
      frameOffset = 0;
      frameWidth = 3;
      frameSpacing = 1;
      useColoredLegendText = true;
      prevFpsFrameTime = std::chrono::steady_clock::now();
      fpsFramesCount = 0;
      avgFrameTime = 1.0f;
    }
    void Render()
    {
      fpsFramesCount++;
      auto currFrameTime = std::chrono::steady_clock::now();
      {
        float fpsDeltaTime = std::chrono::duration<float>(currFrameTime - prevFpsFrameTime).count();
        if (fpsDeltaTime > 0.5f)
        {
          this->avgFrameTime = fpsDeltaTime / float(fpsFramesCount);
          fpsFramesCount = 0;
          prevFpsFrameTime = currFrameTime;
        }
      }

      char title[128];
      std::snprintf(title, sizeof(title), "Legit profiler [%.2ffps\t cpu: %.2fms gpu: %.2fms]###ProfilerWindow",
        1.0f / avgFrameTime, cpuGraph.GetTotalTaskTime(frameOffset) * 1000.0f, gpuGraph.GetTotalTaskTime(frameOffset) * 1000.0f);
      ImGui::Begin(title, 0, ImGuiWindowFlags_NoScrollbar);
      ImVec2 canvasSize = ImGui::GetContentRegionAvail();

      int sizeMargin = int(ImGui::GetStyle().ItemSpacing.y);
      int maxGraphHeight = 300;
      int availableGraphHeight = (int(canvasSize.y) - sizeMargin) / 2;
      int graphHeight = std::min(maxGraphHeight, availableGraphHeight);
      int legendWidth = 200;
      int graphWidth = int(canvasSize.x) - legendWidth;
      gpuGraph.RenderTimings(graphWidth, legendWidth, graphHeight, frameOffset, maxFrameTime);
      cpuGraph.RenderTimings(graphWidth, legendWidth, graphHeight, frameOffset, maxFrameTime);
      if (float(graphHeight * 2 + sizeMargin + sizeMargin) < canvasSize.y)
      {
        ImGui::Columns(2);
        ImGui::Checkbox("Stop profiling", &stopProfiling);
        ImGui::Checkbox("Colored legend text", &useColoredLegendText);
        ImGui::DragInt("Frame offset", &frameOffset, 1.0f, 0, 400);
        ImGui::NextColumn();

        ImGui::SliderInt("Frame width", &frameWidth, 1, 4);
        ImGui::SliderInt("Frame spacing", &frameSpacing, 0, 2);
        ImGui::SliderFloat("Transparency", &ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w, 0.0f, 1.0f);
        ImGui::Columns(1);
      }
      if (!stopProfiling)
        frameOffset = 0;
      gpuGraph.frameWidth = frameWidth;
      gpuGraph.frameSpacing = frameSpacing;
      gpuGraph.useColoredLegendText = useColoredLegendText;
      cpuGraph.frameWidth = frameWidth;
      cpuGraph.frameSpacing = frameSpacing;
      cpuGraph.useColoredLegendText = useColoredLegendText;

      ImGui::End();
    }
    bool stopProfiling;
    int frameOffset;
    ProfilerGraph cpuGraph;
    ProfilerGraph gpuGraph;
    int frameWidth;
    int frameSpacing;
    bool useColoredLegendText;
    using TimePoint = std::chrono::steady_clock::time_point;
    TimePoint prevFpsFrameTime;
    size_t fpsFramesCount;
    float avgFrameTime;
    float maxFrameTime;
  };
}
