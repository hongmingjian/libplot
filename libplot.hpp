#ifndef LIBPLOT_HPP
#define LIBPLOT_HPP
#define _USE_MATH_DEFINES

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>
#include <variant>
#include <iostream>
#include <sstream>
#include <iomanip>


#include "libplot.h"

typedef struct draw_command draw_command;

enum CoordType { COORD_CARTESIAN = 0, COORD_POLAR = 1 };
enum AxisScale { SCALE_LINEAR = 0, SCALE_LOG = 1 };

class CoordSystem {
private:
    void ZoomAxis(double factor, double center, double& rawMin, double& rawMax, enum AxisScale scale) {
        if (scale == SCALE_LOG) {
            double logMouse = std::log10(center <= 0 ? 1e-5 : center);
            double logMin   = std::log10(rawMin);
            double logMax   = std::log10(rawMax);

            double newRange = (logMax - logMin) * factor;
            double ratio    = (logMouse - logMin) / (logMax - logMin);

            rawMin = std::pow(10.0, logMouse - newRange * ratio);
            rawMax = std::pow(10.0, logMouse + newRange * (1.0 - ratio));
        } else {
            double newRange = (rawMax - rawMin) * factor;
            double ratio    = (center - rawMin) / (rawMax - rawMin);

            rawMin = center - newRange * ratio;
            rawMax = center + newRange * (1.0 - ratio);
        }
    }

public:
    int width = 600;
    int height = 450;
    int padL = 70; //left
    int padR = 70; //right
    int padT = 40; //top
    int padB = 50; //bottom

    double rawXMin = 0.0, rawXMax = 10.0;
    double rawLeftYMin = 0.0, rawLeftYMax = 100.0;
    double rawRightYMin = 50.0, rawRightYMax = 1000.0;
    bool hasRightYAxis = false;

    double renderXMin = 0.0, renderXMax = 10.0;          //used as thetaMin, thetaMax
    double renderLeftYMin = 0.0, renderLeftYMax = 100.0; //used as rMin, rMax
    double renderRightYMin = 50.0, renderRightYMax = 1000.0;

    CoordType type = COORD_CARTESIAN;
    AxisScale xScale = SCALE_LINEAR;
    AxisScale leftYScale = SCALE_LINEAR; //used as rScale
    AxisScale rightYScale = SCALE_LINEAR;

    float GetPlotWidth() const { return static_cast<float>(width - padL - padR); }
    float GetPlotHeight() const { return static_cast<float>(height - padT - padB); }

    float ToPixelX(double mathX) const {
        if (xScale == SCALE_LOG) {
            double logX = std::log10(mathX <= 0 ? 1e-5 : mathX);
            double logMin = std::log10(renderXMin <= 0 ? 1e-5 : renderXMin);
            double logMax = std::log10(renderXMax <= 0 ? 1.0 : renderXMax);
            return static_cast<float>(padL) + static_cast<float>((logX - logMin) / (logMax - logMin)) * GetPlotWidth();
        }
        return static_cast<float>(padL) + static_cast<float>((mathX - renderXMin) / (renderXMax - renderXMin)) * GetPlotWidth();
    }

    float ToPixelY(double mathY, AxisType type = AXIS_LEFT) const {
        double minY = (type == AXIS_LEFT) ? renderLeftYMin : renderRightYMin;
        double maxY = (type == AXIS_LEFT) ? renderLeftYMax : renderRightYMax;
        AxisScale scale = (type == AXIS_LEFT) ? leftYScale : rightYScale;

        if (scale == SCALE_LOG) {
            double logY = std::log10(mathY <= 0 ? 1e-5 : mathY);
            double logMin = std::log10(minY <= 0 ? 1e-5 : minY);
            double logMax = std::log10(maxY <= 0 ? 1.0 : maxY);
            return static_cast<float>(height - padB) - static_cast<float>((logY - logMin) / (logMax - logMin)) * GetPlotHeight();
        }

        return static_cast<float>(height - padB) - static_cast<float>((mathY - minY) / (maxY - minY)) * GetPlotHeight();
    }


    double ToMathX(float pixelX) const {
        double pct = (pixelX - padL) / GetPlotWidth();
        if (xScale == SCALE_LOG) {
            double logMin = std::log10(renderXMin <= 0 ? 1e-5 : renderXMin);
            double logMax = std::log10(renderXMax <= 0 ? 1.0 : renderXMax);
            return std::pow(10.0, logMin + pct * (logMax - logMin));
        }
        return renderXMin + pct * (renderXMax - renderXMin);
    }

    double ToMathY(float pixelY, AxisType type = AXIS_LEFT) const {
        double pct = (static_cast<float>(height - padB) - pixelY) / GetPlotHeight();
        double minY = (type == AXIS_LEFT) ? renderLeftYMin : renderRightYMin;
        double maxY = (type == AXIS_LEFT) ? renderLeftYMax : renderRightYMax;
        AxisScale scale = (type == AXIS_LEFT) ? leftYScale : rightYScale;

        if (scale == SCALE_LOG) {
            double logMin = std::log10(minY <= 0 ? 1e-5 : minY);
            double logMax = std::log10(maxY <= 0 ? 1.0 : maxY);
            return std::pow(10.0, logMin + pct * (logMax - logMin));
        }
        return minY + pct * (maxY - minY);
    }

    std::pair<float, float> ToPolarPixel(double theta, double r) const {
        float cx = static_cast<float>(padL) + GetPlotWidth() / 2.0f;
        float cy = static_cast<float>(padT) + GetPlotHeight() / 2.0f;
        float maxPixelR = std::min(GetPlotWidth(), GetPlotHeight()) / 2.0f;
        double minR = renderLeftYMin;
        double maxR = renderLeftYMax;

        float pixelR = 0.0f;
        if (leftYScale == SCALE_LOG) {
            if (minR <= 0)
				minR = 1e-2;
            if (maxR <= minR)
				maxR = 1.0;
            double currentR = (r <= minR) ? minR : r;

            double logMin = std::log10(minR);
            double logMax = std::log10(maxR);
            double logCur = std::log10(currentR);

            pixelR = static_cast<float>((logCur - logMin) / (logMax - logMin)) * maxPixelR;
        } else {
            if (maxR <= minR)
				maxR = minR + 1.0;
            double currentR = (r <= minR) ? minR : r;

            pixelR = static_cast<float>((currentR - minR) / (maxR - minR)) * maxPixelR;
        }

        float px = cx + pixelR * static_cast<float>(std::cos(theta));
        float py = cy - pixelR * static_cast<float>(std::sin(theta));
        return {px, py};
    }

    std::pair<double, double> ToPolarMath(float pixelX, float pixelY) const {
        float cx = static_cast<float>(padL) + GetPlotWidth() / 2.0f;
        float cy = static_cast<float>(padT) + GetPlotHeight() / 2.0f;
        float maxPixelR = std::min(GetPlotWidth(), GetPlotHeight()) / 2.0f;
        double minR = renderLeftYMin;
        double maxR = renderLeftYMax;

        float dx = pixelX - cx;
        float dy = cy - pixelY;

        double theta = std::atan2(dy, dx);
        if (theta < 0)
            theta += 2.0 * M_PI;

        float pixelR = std::sqrt(dx * dx + dy * dy);
        if (pixelR > maxPixelR)
			pixelR = maxPixelR;

        double r = 0.0;

        if (leftYScale == SCALE_LOG) {
            if (minR <= 0)
				minR = 1e-2;
            if (maxR <= minR)
				maxR = 1.0;

            double logMin = std::log10(minR);
            double logMax = std::log10(maxR);
            double pct = pixelR / maxPixelR;

            r = std::pow(10.0, logMin + pct * (logMax - logMin));
        } else {
            double pct = pixelR / maxPixelR;
            r = minR + pct * (maxR - minR);
        }

        return {theta, r};
    }


	/**
	 * @brief Heckbert 寻找人类视觉习惯规整步长（1, 2, 5, 10 规范化）算法
	 */
	double NiceNumber(double x, bool round) {
		int exp = static_cast<int>(std::floor(std::log10(x)));
		double f = x / std::pow(10.0, exp);
		double nf;
		if (round) {
			if (f < 1.5)
				nf = 1.0;
			else if (f < 3.0)
				nf = 2.0;
			else if (f < 7.0)
				nf = 5.0;
			else
				nf = 10.0;
		} else {
			if (f <= 1.0)
				nf = 1.0;
			else if (f <= 2.0)
				nf = 2.0;
			else if (f <= 5.0)
				nf = 5.0;
			else
				nf = 10.0;
		}
		return nf * std::pow(10.0, exp);
	}

	/**
	 * @brief 工业级高鲁棒性数学刻度生成器
	 *
	 * @param minVal      当前轴的数学范围下限
	 * @param maxVal      当前轴的数学范围上限
	 * @param scale       缩放类型：SCALE_LINEAR(线性) 或 SCALE_LOG(对数)
	 * @param count 期望在屏幕上生成的黄金刻度线数量
	 * @param isCircular  是否为圆周闭环轴（专门用于极坐标 360° 拦截，防止 0°/360° 重叠鬼影）
	 * @return std::vector<double> 规整、高精度、无精度漂移的数学刻度集合
	 */
	std::vector<double> CalculateTicks(double minVal, double maxVal, enum AxisScale scale, int count, bool isCircular = false) {
		std::vector<double> ticks;

		// ------------------------------------------------------------------------
		// 防线 1：极强力拦截“零范围（Zero Range）”引发的除零无限死锁 (CPU 100% 挂死)
		// ------------------------------------------------------------------------
		if (std::abs(maxVal - minVal) < 1e-6) {
			if (minVal == 0.0) {
				minVal = -1.0;
				maxVal = 1.0;
			} else {
				// 如果不是 0，向两边强行撑开 10% 的物理视口空间
				minVal *= 0.9;
				maxVal *= 1.1;
			}
		}
		if (count < 2)
			count = 2; // 至少保证有首尾两个刻度

		// ------------------------------------------------------------------------
		// 流派 A：升级后的对数空间动态刻度流（完美解决高频放大时网格消失的问题）
		// ------------------------------------------------------------------------
		if (scale ==  SCALE_LOG ) {
			// 对数安全截断：严防 log10(0) 或负数导致数学崩溃
			double safeMin = (minVal <= 0) ? 1e-5 : minVal;
			double safeMax = (maxVal <= safeMin) ? safeMin + 1.0 : maxVal;

			double logMin = std::log10(safeMin);
			double logMax = std::log10(safeMax);
			double logRange = logMax - logMin;

			// 流派 A-1：如果对数跨度大于等于 1.0 个数量级，使用标准的 10 整数幂步长 (1.0f)
			if (logRange >= 1.0) {
				double startLog = std::floor(logMin);
				double endLog = std::ceil(logMax);

				// 【硬核技巧】：采用纯整型计数器安全迭代，彻底灭绝浮点数累加漂移！
				int total_steps = static_cast<int>(endLog - startLog);
				for (int i = 0; i <= total_steps; ++i) {
					double val = std::pow(10.0, startLog + i * 1.0);
					// 严苛边界拦截：剔除超出实际 render 范围的冗余外扩点
					if (val >= minVal && val <= maxVal) {
						ticks.push_back(val);
					}
				}
			// 流派 A-2：如果对数区间被极度压缩（用户疯狂滚轮放大），自动切换为对数空间内的“二级细分网格”
			} else {
				// 在对数域 [logMin, logMax] 内无脑套用 Heckbert 线性步长算法
				double logStep = NiceNumber(logRange / (count - 1), true);
				if (logStep <= 0)
					logStep = 0.1; // 极限防御

				double logStart = std::floor(logMin / logStep) * logStep;
				int total_steps = static_cast<int>(std::floor((logMax - logStart) / logStep)) + 2;

				for (int i = 0; i <= total_steps; ++i) {
					double currentLog = logStart + i * logStep;
					double val = std::pow(10.0, currentLog);

					if (val >= minVal && val <= maxVal) {
						ticks.push_back(val);
					}
				}
			}
		} else {
			double range = maxVal - minVal;
			double step = NiceNumber(range / (count - 1), true);
			if (step <= 0)
				return ticks; // 双重保险防御

			// 规范化起始迭代数学点
			double start = std::floor(minVal / step) * step;

			// 科学精算最大需要循环的安全次数
			int max_steps = static_cast<int>(std::floor((maxVal - start) / step)) + 2;

			// 【核心修正】：使用纯整型 i 作为主驱动，val 永远由首项加上偏移量精准求得
			for (int i = 0; i <= max_steps; ++i) {
				double val = start + i * step;

				// 精度修正：如果算出的数字极其接近 0，强制归零，防止屏幕上刷出 "-0.0000" 的尴尬错字现象
				if (std::abs(val) < (step * 1e-6)) {
					val = 0.0;
				}

				// 防线 2：严格拦截因为 Heckbert 步长外扩而飞出画布裁剪区之外的越界点
				if (val >= minVal && val <= maxVal) {

					// 【圆周周期性拦截补丁】：如果是极坐标圆周轴，且当前值极其接近或等于 maxVal (360°)，
					// 它在圆周几何上和最开始的 0° 完全物理重合，直接拦截踢出，绝对不放行！
					if (isCircular && std::abs(val - maxVal) < (step * 1e-4)) {
						continue;
					}

					ticks.push_back(val);
				}
			}
		}

		return ticks;
	}

    void zoom(int cx, int cy, double factor) {
        if (type == COORD_POLAR) {
			auto c = ToPolarMath(cx, cy);
            ZoomAxis(factor, c.second, rawLeftYMin, rawLeftYMax, leftYScale);

            if (false/*GetOpt("zoomThetaEnabled", 0.0) == 1.0*/) {
                ZoomAxis(factor, c.first, rawXMin, rawXMax, xScale);
            }
        } else {
            ZoomAxis(factor, ToMathX(cx), rawXMin, rawXMax, xScale);
            ZoomAxis(factor, ToMathY(cy, AXIS_LEFT), rawLeftYMin, rawLeftYMax, leftYScale);
            if (hasRightYAxis) {
                ZoomAxis(factor, ToMathY(cy, AXIS_RIGHT), rawRightYMin, rawRightYMax, rightYScale);
            }
        }
        ApplyMarginPadding(true/*GetOpt("enablePadding", 1.0) == 1.0*/, 0.08f/*GetOpt("paddingRatio", 0.08)*/);
    }

    void ApplyMarginPadding(bool enable, double ratio) {
        if (!enable || type == COORD_POLAR) {
            renderXMin = rawXMin;
			renderXMax = rawXMax;
            renderLeftYMin = rawLeftYMin;
			renderLeftYMax = rawLeftYMax;
            renderRightYMin = rawRightYMin;
			renderRightYMax = rawRightYMax;
            return;
        }
        double dx = (rawXMax - rawXMin) * ratio;
        renderXMin = (xScale == SCALE_LOG) ? rawXMin : rawXMin - dx;
        renderXMax = rawXMax + dx;

        double dyl = (rawLeftYMax - rawLeftYMin) * ratio;
        renderLeftYMin = (leftYScale == SCALE_LOG) ? rawLeftYMin : rawLeftYMin - dyl;
        renderLeftYMax = rawLeftYMax + dyl;

        if (hasRightYAxis) {
            double dyr = (rawRightYMax - rawRightYMin) * ratio;
            renderRightYMin = (rightYScale == SCALE_LOG) ? rawRightYMin : rawRightYMin - dyr;
            renderRightYMax = rawRightYMax + dyr;
        }
    }

};

class Figure {
private:
	std::map<std::string, std::variant<std::monostate, int, float, std::string> > options;
    CoordSystem coord;

	char *safe_strncpy(char *buf, const char *src, size_t bufsize)
	{
		std::strncpy(buf, src, bufsize - 1);
		buf[bufsize - 1] = '\0';
		return buf;
	}

    std::string FormatNumber(double val, AxisScale scale)
	{
        std::ostringstream oss;
        if (scale == SCALE_LOG && (val >= 1000.0 || val <= 0.001)) {
            oss << std::scientific << std::setprecision(0) << val;
            return oss.str();
        }
        oss << std::setprecision(4) << val;
        return oss.str();
    }

	int drawCoord(struct draw_command *out, size_t nout) {
		std::vector<struct draw_command> cmds;
        int xTicksCount = 6;//static_cast<int>(GetOpt("xTargetTicks", 6.0));
        int leftYTicksCount = 5;//static_cast<int>(GetOpt("leftYTargetTicks", 5.0));

		if (coord.type == COORD_POLAR) {
				// [A-1. 同心圆网格生成] (保持我们刚刚用 ToPolarPixel(0,0) 修正后的完美版本)
				std::vector<double> rTicks = coord.CalculateTicks(coord.renderLeftYMin, coord.renderLeftYMax, SCALE_LINEAR, leftYTicksCount);
				std::pair<float, float> center = coord.ToPolarPixel(0.0, 0.0);
				float cx = center.first;
				float cy = center.second;

				for (double r : rTicks) {
					if (r <= coord.renderLeftYMin)
						continue;
					draw_command c1{}; c1.op = OP_SET_TEXT_COLOR; c1.zIndex = 10; safe_strncpy(c1.argc1, "#E5E5E5", sizeof(c1.argc1)); cmds.push_back(c1);
					std::pair<float, float> edge = coord.ToPolarPixel(0.0, r);
					float pixelR = std::abs(edge.first - cx);
					if (pixelR == 0)
						pixelR = std::abs(edge.second - cy);
					draw_command c2{}; c2.op = OP_DRAW_CIRCLE; c2.zIndex = 10; c2.x = cx; c2.y = cy; c2.argf1 = pixelR; cmds.push_back(c2);

					draw_command c3{}; c3.op = OP_DRAW_TEXT; c3.zIndex = 20; c3.x = edge.first; c3.y = edge.second - 2.0f;
					safe_strncpy(c3.argc1, FormatNumber(r, SCALE_LINEAR).c_str(), sizeof(c3.argc1)); cmds.push_back(c3);
				}
				// ====================================================================
				// 【顶级重构】：[A-2. 360度角度网格射线与外圈角度文本生成]
				// ====================================================================
				std::vector<double> thetaTicks = coord.CalculateTicks(0.0, 360.0, SCALE_LINEAR, xTicksCount, true);

				for (double angle_deg : thetaTicks) {
					// 1. 将动态的角度度数 转换为 ToPolarPixel 接收的数学弧度 (Radians)
					double theta = angle_deg * M_PI / 180.0;

					// 2. 映射射线最外圈端点的物理像素位置
					std::pair<float, float> outer_edge = coord.ToPolarPixel(theta, coord.renderXMax);

					// 3. 压入射线虚线指令（图层 = 10）
					draw_command r1{}; r1.op = OP_SET_TEXT_COLOR; r1.zIndex = 10; safe_strncpy(r1.argc1, "#E5E5E5", sizeof(r1.argc1)); cmds.push_back(r1);
					draw_command r2{}; r2.op = OP_SET_LINE_STYLE; r2.zIndex = 10; safe_strncpy(r2.argc1, ".-", sizeof(r2.argc1)); cmds.push_back(r2);
					draw_command r3{}; r3.op = OP_MOVETO; r3.zIndex = 10; r3.x = cx; r3.y = cy; cmds.push_back(r3);
					draw_command r4{}; r4.op = OP_LINETO; r4.zIndex = 10; r4.x = outer_edge.first; r4.y = outer_edge.second; cmds.push_back(r4);

					// 4. 映射角度文本的绝对像素坐标（在外圈外侧物理向外放射外推 15.0f 像素）
					float text_px = outer_edge.first + 15.0f * static_cast<float>(std::cos(theta));
					float text_py = outer_edge.second - 15.0f * static_cast<float>(std::sin(theta));

					// 5. 构造刻度文本内容（使用我们 FormatNumber 规整格式化，并追加度数符号）
					std::string label_str = FormatNumber(angle_deg, SCALE_LINEAR) + "°";

					// 6. 压入外圈角度值指令（图层 = 20）
					draw_command r5{}; r5.op = OP_SET_TEXT_COLOR; r5.zIndex = 20; safe_strncpy(r5.argc1, "#666666", sizeof(r5.argc1)); cmds.push_back(r5);
					draw_command r6{}; r6.op = OP_SET_LINE_STYLE; r6.zIndex = 20; safe_strncpy(r6.argc1, "-", sizeof(r6.argc1)); cmds.push_back(r6);
					draw_command r7{}; r7.op = OP_DRAW_TEXT; r7.zIndex = 20; r7.x = text_px; r7.y = text_py; safe_strncpy(r7.argc1, label_str.c_str(), sizeof(r7.argc1)); cmds.push_back(r7);
				}

				// 7. 最外圈物理外壳保护实线圈（图层 = 22）
				std::pair<float, float> max_edge = coord.ToPolarPixel(0.0, coord.renderXMax);
				float max_pixel_r = std::abs(max_edge.first - cx);
				if (max_pixel_r == 0)
					max_pixel_r = std::abs(max_edge.second - cy);

				draw_command b1{}; b1.op = OP_SET_TEXT_COLOR; b1.zIndex = 22; safe_strncpy(b1.argc1, "#000000", sizeof(b1.argc1)); cmds.push_back(b1);
				draw_command b2{}; b2.op = OP_SET_LINE_WIDTH; b2.zIndex = 22; b2.argf1 = 1.5f; cmds.push_back(b2);
				draw_command b3{}; b3.op = OP_DRAW_CIRCLE; b3.zIndex = 22; b3.x = cx; b3.y = cy; b3.argf1 = max_pixel_r; cmds.push_back(b3);

		} else if (coord.type ==  COORD_CARTESIAN) {

				float pxMin = coord.ToPixelX(coord.renderXMin);
				float pxMax = coord.ToPixelX(coord.renderXMax);
				float pyMin = coord.ToPixelY(coord.renderLeftYMin, AXIS_LEFT);
				float pyMax = coord.ToPixelY(coord.renderLeftYMax, AXIS_LEFT);

				double xTilt = 0;//GetOpt("xLabelAngle", 0.0);
				std::vector<double> xTicks = coord.CalculateTicks(coord.renderXMin, coord.renderXMax, coord.xScale, xTicksCount);
				for (double val : xTicks) {
					float px = coord.ToPixelX(val);

					cmds.push_back(draw_command{OP_SET_TEXT_COLOR, 0.0f, 0.0f, 0.0f, "#E5E5E5", 10});
					cmds.push_back(draw_command{OP_SET_LINE_STYLE, 0.0f, 0.0f, 0.0f, ":", 10});
					cmds.push_back(draw_command{OP_MOVETO, px, pyMin, 0.0f, "", 10});
					cmds.push_back(draw_command{OP_LINETO, px, pyMax, 0.0f, "", 10});

					cmds.push_back(draw_command{OP_SET_TEXT_COLOR, 0.0f, 0.0f, 0.0f, "#333333", 10});
					cmds.push_back(draw_command{OP_SET_LINE_STYLE, 0.0f, 0.0f, 0.0f, ":", 10});
					cmds.push_back(draw_command{OP_DRAW_TEXT, px, pyMin+15.0f, 0.0f, "", 10});
					safe_strncpy(cmds.back().argc1, FormatNumber(val, coord.xScale).c_str(), sizeof(draw_command::argc1));
				}

				std::vector<double> leftYTicks = coord.CalculateTicks(coord.renderLeftYMin, coord.renderLeftYMax, coord.leftYScale, leftYTicksCount);
				for (double val : leftYTicks) {
					float py = coord.ToPixelY(val, AXIS_LEFT);

					cmds.push_back(draw_command{OP_SET_TEXT_COLOR, 0.0f, 0.0f, 0.0f, "#E5E5E5", 10});
					cmds.push_back(draw_command{OP_SET_LINE_STYLE, 0.0f, 0.0f, 0.0f, ":", 10});
					cmds.push_back(draw_command{OP_MOVETO, pxMin, py, 0.0f, "", 10});
					cmds.push_back(draw_command{OP_LINETO, pxMax, py, 0.0f, "", 10});

					cmds.push_back(draw_command{OP_SET_TEXT_COLOR, 0.0f, 0.0f, 0.0f, "#0000FF", 10});
					cmds.push_back(draw_command{OP_SET_LINE_STYLE, 0.0f, 0.0f, 0.0f, ":", 10});
					cmds.push_back(draw_command{OP_DRAW_TEXT, pxMin-45.0f, py-6.0f, 0.0f, "", 10});
					safe_strncpy(cmds.back().argc1, FormatNumber(val, coord.xScale).c_str(), sizeof(draw_command::argc1));
				}

				if (coord.hasRightYAxis) {
					int rightYTicksCount = 5;//static_cast<int>(GetOpt("rightYTargetTicks", 5.0));
					std::vector<double> rightYTicks = coord.CalculateTicks(coord.renderRightYMin, coord.renderRightYMax, coord.rightYScale, rightYTicksCount);

					for (double val : rightYTicks) {
						float py = coord.ToPixelY(val, AXIS_RIGHT);

						// 如果不希望右轴的横线和左轴打架，可以不画这根虚线，这里画出来可以供参考
						draw_command c1{}; c1.op = OP_SET_TEXT_COLOR; safe_strncpy(c1.argc1, "#F0F0F0", sizeof(c1.argc1)); c1.zIndex = 9; cmds.push_back(c1);
						draw_command c2{}; c2.op = OP_SET_LINE_STYLE; safe_strncpy(c2.argc1, ".", sizeof(c2.argc1)); c2.zIndex = 9; cmds.push_back(c2);
						draw_command c3{}; c3.op = OP_MOVETO; c3.x = pxMin; c3.y = py; c3.zIndex = 9; cmds.push_back(c3);
						draw_command c4{}; c4.op = OP_LINETO; c4.x = pxMax; c4.y = py; c4.zIndex = 9; cmds.push_back(c4);

						// 右轴文本：挂在 pxMax 右侧，物理顺移 +10px
						draw_command c5{}; c5.op = OP_SET_TEXT_COLOR; safe_strncpy(c5.argc1, "#008000", sizeof(c5.argc1)); c5.zIndex = 20; cmds.push_back(c5); // 用绿色区分右轴文本
						draw_command c6{}; c6.op = OP_SET_LINE_STYLE; safe_strncpy(c6.argc1, "-", sizeof(c6.argc1)); c6.zIndex = 20; cmds.push_back(c6);
						draw_command c7{}; c7.op = OP_DRAW_TEXT; c7.x = pxMax + 10.0f; c7.y = py - 6.0f;
						safe_strncpy(c7.argc1, FormatNumber(val, coord.rightYScale).c_str(), sizeof(c7.argc1));
						c7.zIndex = 20;
						cmds.push_back(c7);
					}
				}

				cmds.push_back(draw_command{OP_SET_LINE_STYLE, 0.0f, 0.0f, 0.0f, "-", 22});
				cmds.push_back(draw_command{OP_SET_TEXT_COLOR, 0.0f, 0.0f, 0.0f, "#000000", 22});
				cmds.push_back(draw_command{OP_SET_LINE_WIDTH, 0.0f, 0.0f, 1.5f, "", 22});
				cmds.push_back(draw_command{OP_MOVETO, pxMin, pyMax, 0.0f, "", 22});
				cmds.push_back(draw_command{OP_LINETO, pxMin, pyMin, 0.0f, "", 22});
				cmds.push_back(draw_command{OP_LINETO, pxMax, pyMin, 0.0f, "", 22});
				if (coord.hasRightYAxis)
					cmds.push_back(draw_command{OP_LINETO, pxMax, pyMax, 0.0f, "", 22});
		}

		int nret = std::min(cmds.size(), nout);
		for (int i = 0; i < nret; i++) {
			out[i] = cmds.data()[i];
		}
		return nret;
	}
public:
	Figure() {
	}

    void SetMathLimits(double x1, double x2, double y1, double y2) {
        coord.rawXMin = x1;
		coord.rawXMax = x2;
		coord.rawLeftYMin = y1;
		coord.rawLeftYMax = y2;
        bool enablePad = true;//GetOpt("enablePadding", 1.0);
        float padRatio = 0.08f;//GetOpt("paddingRatio", 0.08);
        coord.ApplyMarginPadding(enablePad, padRatio);
    }

	int plot(const std::vector<float> &x, const std::vector<float> &y, struct draw_command *out, int nout) {
        coord.ApplyMarginPadding(true, 0.08f);
		return drawCoord(out, nout);
	}

	int set_option(const std::string &option, const std::string &value) {
		if (option == "figure.size") {
			int w, h;
			if (sscanf(value.c_str(), "%dx%d", &w, &h) == 2) {
				coord.width = w;
				coord.height = h;
				return 0;
			}
		}

		return -1;
	}

    void zoom(int cx, int cy, double factor) {
		return coord.zoom(cx, cy, factor);
    }

	void pan(int dx, int dy) {
		float x = coord.ToMathX(dx) - coord.ToMathX(0);
		float y = coord.ToMathY(dy) - coord.ToMathY(0);
		SetMathLimits(coord.rawXMin - x, coord.rawXMax - x, coord.rawLeftYMin - y, coord.rawLeftYMax - y);
	}

	void pixel2logical(int px, int py, float *lx, float *ly, float *ly2) {
		*lx = coord.ToMathX(px);
		*ly = coord.ToMathY(py, AXIS_LEFT);
		if (coord.hasRightYAxis)
			*ly2 = coord.ToMathY(py, AXIS_RIGHT);
	}
};
#endif // LIBPLOT_HPP
