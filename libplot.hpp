#ifndef LIBPLOT_HPP
#define LIBPLOT_HPP
#define _USE_MATH_DEFINES

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <iomanip>


#include "libplot.h"

typedef struct draw_command draw_command;

class Figure;

enum AxisScale { SCALE_LINEAR = 0, SCALE_LOG = 1 };

enum MarkerType {
    MARKER_NONE = 0,
    MARKER_X,        // 'x'
    MARKER_PLUS,     // '+'
    MARKER_CIRCLE,   // 'o'
    MARKER_SQUARE,   // 's'
    MARKER_TRIANGLE, // '^'
    MARKER_DIAMOND   // 'd'
};

struct TickConfig {
    bool visible = true;
    std::string color = "#000000";
    std::string font; // "Arial,12"
};

struct GridConfig {
    bool visible = true;
    std::string color = "#E0E0E0";
    std::string style = ":";
};

struct Axis {
	Figure *figure = nullptr;

	//std::string name = "";
    std::string label = "";

    AxisScale scale = SCALE_LINEAR;
    float min = 0.0f;
    float max = 100.0f;
	float ratioMargin = 0.08f;

    bool visible = true;
	float width = 1.0f;
    std::string color = "#000000";

    float tickInterval = 0.0f; // 0 for auto ticks
    TickConfig tickConfig;
    GridConfig gridConfig;

    bool panEnabled = true;
    bool zoomEnabled = true;

	void applyMargin()
	{
        if (ratioMargin <= 0.0f)
			return;

		double d;
        if (scale == SCALE_LOG) {
            double logMin = std::log10(min);
            double logMax = std::log10(max);
			d = (logMax - logMin) * ratioMargin;
            min = std::pow(10.0, logMin + d);
            max = std::pow(10.0, logMax + d);
		} else {
			d = (max - min) * ratioMargin;
			min -= d;
			max += d;
		}
    }

    void pan(float d)
	{
        if (!panEnabled)
			return;
		if (!std::isnormal(d))
			return;

        if (scale == SCALE_LOG) {
            min = std::pow(10.0, std::log10(min) + d);
            max = std::pow(10.0, std::log10(max) + d);
        } else {
            min += d;
            max += d;
        }
    }

	void zoom(float center, float factor)
	{
		if (!zoomEnabled)
			return;
		if (factor <= 0)
			return;
		if (!std::isnormal(max - min))
			return;

		double newMin = min;
		double newMax = max;

		if (scale == SCALE_LOG) {
			newMin = std::log10(newMin);
			newMax = std::log10(newMax);
			center = std::log10(center);
		}

		newMin = center * (1 - factor) + newMin * factor;
		newMax = center * (1 - factor) + newMax * factor;

		if (scale == SCALE_LOG) {
			newMin = std::pow(10.0, newMin);
			newMax = std::pow(10.0, newMax);
		}

		if (newMax <= newMin || !std::isnormal(newMax - newMin))
			return;

		min = newMin;
		max = newMax;
	}

	float NiceNumber(float x, bool round) const
	{
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

	template<typename T = int>
	std::vector<float> calculateTicks(int count) const
	{
		std::vector<float> ticks;

		if (count < 2)
			count = 2;

		if (figure && figure->coord == COORD_POLAR && this == &(figure->axisX)) {
			int divisions = (count > 4) ? 12 : 4;
			for (int i = 0; i < divisions; ++i)
				ticks.push_back(2 * M_PI * i / divisions);
			return ticks;
		}

		if (figure && this == &(figure->axisY2)) {
			std::vector<float> leftTicks = figure->axisY.calculateTicks(count);

			float leftMin = figure->axisY.min;
			float leftMax = figure->axisY.max;
			float leftRange = leftMax - leftMin;

			if (std::isnormal(leftRange)) {
				if (scale == SCALE_LOG) {
					double logMin2 = std::log10(min);
					double logMax2 = std::log10(max);
					for (float leftVal : leftTicks)
						ticks.push_back(std::pow(10.0, logMin2 + (logMax2 - logMin2) * (leftVal - leftMin) / leftRange));
				} else {
					for (float leftVal : leftTicks)
						ticks.push_back(min + (max - min) * (leftVal - leftMin) / leftRange);
				}
			}
			return ticks;
		}

		if (scale == SCALE_LOG) {
			double logMin = std::log10(min);
			double logMax = std::log10(max);
			double decadeRange = logMax - logMin;

			if (decadeRange >= 1.0) {
				int startExponent = (int)std::floor(logMin);
				int endExponent   = (int)std::ceil(logMax);
				int exponentStep = 1;
				if (endExponent - startExponent > count && count > 1) {
					exponentStep = (endExponent - startExponent) / (count - 1);
					if (exponentStep < 1)
						exponentStep = 1;
				}

				double displayMin = std::pow(10.0, startExponent);
				double displayMax = std::pow(10.0, endExponent);

				for (int expv = startExponent; expv <= endExponent; expv += exponentStep) {
					double val = std::pow(10.0, expv);
					if (val >= displayMin - 1e-6 && val <= displayMax + 1e-6) {
						if (val >= min && val <= max)
							ticks.push_back((float)val);
					}
				}
			} else {
				double baseRange = NiceNumber(max - min, false);
				double niceStep  = NiceNumber(baseRange / (count - 1), true);

				double displayMin = std::floor(min / niceStep) * niceStep;
				double displayMax = std::ceil(max / niceStep) * niceStep;

				double start = displayMin;
				double end   = displayMax;
				for (double v = start; v <= end + (niceStep * 0.0001); v += niceStep) {
					if (v >= min - (niceStep * 0.0001) && v <= max + (niceStep * 0.0001)) {
						ticks.push_back((float)v);
					}
				}
			}
			return ticks;
		}

		double niceStep;
		if (tickInterval > 0.0f) {
			niceStep = tickInterval;
		} else {
			double range = NiceNumber(max - min, false);
			niceStep = NiceNumber(range / (count - 1), true);
		}

		double start = std::floor(min / niceStep) * niceStep;
		double end   = std::ceil(max / niceStep) * niceStep;
		for (double v = start; v <= end + (niceStep * 0.0001); v += niceStep) {
			if (v >= min - (niceStep * 0.0001) && v <= max + (niceStep * 0.0001)) {
				ticks.push_back((float)v);
			}
		}

		return ticks;
	}
};

struct Curve {
	Figure *figure = nullptr;

    std::string name = "";
    std::string label = "";

    std::vector<float> xData;
    std::vector<float> yData;

    std::string color = "#000000";
    std::string style = "-";
    float width = 1.5f;

	MarkerType markerType = MARKER_NONE; // None for no marker
	float markerSize = 6.0f; // in pixels
	std::string markerColor = "#000000";
	bool markerFilled = false;

    int set_option(const std::string& opt, const std::string& val) {
		return 0;
	}
};

class Figure {
public:
	int coord = COORD_CARTESIAN;
	float width = 500.0f; //in pixels
	float height = 400.0f; //in pixels
	//std::string bgColor = "#FFFFFF";
	std::map<std::string, float> paddings = { {"L", 60}, {"T", 40}, {"R", 60}, {"B", 40}};

	Axis axisX;   // as theta in Polar
	Axis axisY;   // primary (left) Y, as r in Polar
	Axis axisY2;  // secondary (right) Y

	std::vector<Curve *> curves;

    void applyMargin() {
        if (coord == COORD_POLAR)
            return;
        axisX.applyMargin();
        axisY.applyMargin();
        if (axisY2.visible)
            axisY2.applyMargin();
    }

    int set_option(const std::string& option, const std::string& value) {
		if (option == "figure.size") {
			int w, h;
			if (sscanf(value.c_str(), "%dx%d", &w, &h) == 2) {
				width = w;
				height = h;
				return 0;
			}
		}
		return -1;
	}

    Figure(int coord_ = COORD_CARTESIAN) {
		coord = coord_;

		axisX.figure = this;
        axisY.figure = this;
        axisY2.figure = this;
		axisY2.visible = false;

		axisY.min = 1;//std::numeric_limits<float>::min();
		axisY.max = 1e2;
		//axisY.scale = SCALE_LOG;

		if (coord == COORD_POLAR) {
			axisX.min = 0.0f;
			axisX.max = 2 * M_PI;

			axisX.panEnabled = false;
			axisX.zoomEnabled = false;
		}
		applyMargin();
    }

    ~Figure() {
        for (Curve *c : curves)
            delete c;
        curves.clear();
    }

    float GetPlotWidth() const { return width - paddings.at("L") - paddings.at("R"); }
    float GetPlotHeight() const { return height - paddings.at("T") - paddings.at("B"); }

    float ToPixelX(float mathX) const {
		if (!std::isnormal(axisX.max - axisX.min))
			return std::numeric_limits<float>::quiet_NaN();

        if (axisX.scale == SCALE_LOG) {
            double logX = std::log10(mathX);
            double logMin = std::log10(axisX.min);
            double logMax = std::log10(axisX.max);

            return paddings.at("L") + GetPlotWidth() * (logX - logMin) / (logMax - logMin);
        }
        return paddings.at("L") + GetPlotWidth() * (mathX - axisX.min) / (axisX.max - axisX.min);
    }

    float ToPixelY(float mathY, int which = AXIS_PRIMARY) const {
		const Axis& axis = (which == AXIS_SECONDARY) ? axisY2 : axisY;

		if (!std::isnormal(axis.max - axis.min))
			return std::numeric_limits<float>::quiet_NaN();

        if (axis.scale == SCALE_LOG) {
            double logY = std::log10(mathY);
            double logMin = std::log10(axis.min);
            double logMax = std::log10(axis.max);
            return (height - paddings.at("B")) - GetPlotHeight() * (logY - logMin) / (logMax - logMin);
        }
        return (height - paddings.at("B")) - GetPlotHeight() * (mathY - axis.min) / (axis.max - axis.min);
    }

    float ToMathX(float pixelX) const {
        if (axisX.scale == SCALE_LOG) {
            double logMin = std::log10(axisX.min);
            double logMax = std::log10(axisX.max);
            return std::pow(10.0, logMin + (logMax - logMin) * (pixelX - paddings.at("L")) / GetPlotWidth());
        }
        return axisX.min + (axisX.max - axisX.min) * (pixelX - paddings.at("L")) / GetPlotWidth();
    }

    float ToMathY(float pixelY, int which = AXIS_PRIMARY) const {
		const Axis& axis = (which == AXIS_SECONDARY) ? axisY2 : axisY;
        if (axis.scale == SCALE_LOG) {
            double logMin = std::log10(axis.min);
            double logMax = std::log10(axis.max);
            return std::pow(10.0, logMin + (logMax - logMin) * (height - paddings.at("B") - pixelY) / GetPlotHeight());
        }
        return axis.min + (axis.max - axis.min) * (height - paddings.at("B") - pixelY) / GetPlotHeight();
    }

    std::pair<float, float> ToPolarPixel(float theta, float r) const {
        double cx = paddings.at("L") + GetPlotWidth() / 2.0f;
        double cy = paddings.at("T") + GetPlotHeight() / 2.0f;

		if (r == 0.0f)
			return {cx, cy};

		if (!std::isnormal(axisY.max - axisY.min))
			return {std::numeric_limits<float>::quiet_NaN(),
				    std::numeric_limits<float>::quiet_NaN()};

		double rmin, rmax;
        if (axisY.scale == SCALE_LOG) {
            r = std::log10(r);
            rmin = std::log10(axisY.min);
            rmax = std::log10(axisY.max);
        } else {
			rmin = axisY.min;
			rmax = axisY.max;
        }

        double maxPixelR = std::min(GetPlotWidth(), GetPlotHeight()) / 2.0f;
        return {cx + maxPixelR * (r - rmin) / (rmax - rmin) * std::cos(theta),
                cy - maxPixelR * (r - rmin) / (rmax - rmin) * std::sin(theta)};
    }

    std::pair<float, float> ToPolarMath(float pixelX, float pixelY) const {
        double cx = paddings.at("L") + GetPlotWidth() / 2.0f;
        double cy = paddings.at("T") + GetPlotHeight() / 2.0f;
        double maxPixelR = std::min(GetPlotWidth(), GetPlotHeight()) / 2.0f;

        double dx = pixelX - cx;
        double dy = cy - pixelY;
        float theta = std::atan2(dy, dx);
        if (theta < 0)
            theta += 2.0 * M_PI;
        float r;

        if (axisY.scale == SCALE_LOG) {
            double logMin = std::log10(axisY.min);
            double logMax = std::log10(axisY.max);

            r = std::pow(10.0, logMin + (logMax - logMin) * std::sqrt(dx * dx + dy * dy) / maxPixelR);
        } else {
            r = axisY.min + (axisY.max - axisY.min) * std::sqrt(dx * dx + dy * dy) / maxPixelR;
        }

        return {theta, r};
    }

	void pixel2logical(int pixelX, int pixelY, float *lx, float *ly, float *ly2) const {
        if (coord == COORD_POLAR) {
			auto mathXY = ToPolarMath(pixelX, pixelY);
			*lx = mathXY.first;
			*ly = mathXY.second;
		} else {
			*lx = ToMathX(pixelX);
			*ly = ToMathY(pixelY);
			if (axisY2.visible)
				*ly2 = ToMathY(pixelY, AXIS_SECONDARY);
		}
	}

    void pan(int pixelXDelta, int pixelYDelta)
	{
        if (pixelXDelta == 0 && pixelYDelta == 0)
			return;

        double plotWidth  = GetPlotWidth();
        double plotHeight = GetPlotHeight();

        if (pixelXDelta != 0) {
            if (axisX.scale == SCALE_LOG) {
                axisX.pan(-(std::log10(axisX.max) - std::log10(axisX.min)) * pixelXDelta / plotWidth);
            } else {
                axisX.pan(-(ToMathX(pixelXDelta) - ToMathX(0.0f)));
            }
        }

        if (pixelYDelta != 0) {
            if (axisY.scale == SCALE_LOG) {
                if (coord == COORD_POLAR) {
					axisY.pan(std::log10(ToPolarMath(pixelXDelta, pixelYDelta).second) - std::log10(ToPolarMath(0.0f, 0.0f).second));
                } else {
					axisY.pan((std::log10(axisY.max) - std::log10(axisY.min)) * pixelYDelta / plotHeight);
                }
            } else {
                if (coord == COORD_POLAR) {
                    axisY.pan(ToPolarMath(pixelXDelta, pixelYDelta).second - ToPolarMath(0.0f, 0.0f).second);
                } else {
                    axisY.pan(-(ToMathY(pixelYDelta) - ToMathY(0.0f)));
                }
            }

			if (axisY2.visible) {
				if (axisY2.scale == SCALE_LOG) {
					axisY2.pan((std::log10(axisY2.max) - std::log10(axisY2.min)) * pixelYDelta / plotHeight);
				} else {
					axisY2.pan(-(ToMathY(pixelYDelta, AXIS_SECONDARY) - ToMathY(0.0f, AXIS_SECONDARY)));
				}
			}
        }
    }

    void zoom(int pixelX, int pixelY, float factor)
	{
		axisX.zoom(ToMathX(pixelX), factor);
		if (coord == COORD_POLAR)
			axisY.zoom(axisY.min, factor); // only zoom in and out around the center
		else
			axisY.zoom(ToMathY(pixelY), factor);
		if (axisY2.visible)
			axisY2.zoom(ToMathY(pixelY, AXIS_SECONDARY), factor);
    }

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

	int show(struct draw_command *out, size_t nout)
	{
		std::vector<struct draw_command> cmds;
        int xTicksCount = 10;//static_cast<int>(GetOpt("xTargetTicks", 6.0));
        int yTicksCount = 5;//static_cast<int>(GetOpt("leftYTargetTicks", 5.0));

		if (coord == COORD_POLAR) {
			auto [cx, cy] = ToPolarPixel(0.0, 0.0);

			auto yTickValues = axisY.calculateTicks(yTicksCount);
			for (float r : yTickValues) {
				if (r <= axisY.min)
					continue;
				std::pair<float, float> edge = ToPolarPixel(0.0, r);
				float pixelR = std::abs(edge.first - cx);
				if (pixelR > 0) {
					cmds.push_back(draw_command{OP_SET_LINE_STYLE, 0.0f, 0.0f, 0.0f, ":", 10});
					cmds.push_back(draw_command{OP_DRAW_CIRCLE, cx, cy, pixelR, "", 10});
					cmds.push_back(draw_command{OP_SET_TEXT_COLOR, 0.0f, 0.0f, 0.0f, "#C0C0C0", 10});
					cmds.push_back(draw_command{OP_DRAW_TEXT, edge.first, edge.second-2.0f, 0.0f, "", 10});
					safe_strncpy(cmds.back().argc1, FormatNumber(r, SCALE_LINEAR).c_str(), sizeof(draw_command::argc1));
				}
			}

			auto xTickValues = axisX.calculateTicks(xTicksCount);
			for (float theta : xTickValues) {
				if (theta <= axisX.min)
					continue;
				std::pair<float, float> outer_edge = ToPolarPixel(theta, axisY.max);

				cmds.push_back(draw_command{OP_SET_LINE_COLOR, 0.0f, 0.0f, 0.0f, "#C0C0C0", 10});
				cmds.push_back(draw_command{OP_SET_LINE_STYLE, 0.0f, 0.0f, 0.0f, ":", 10});
				cmds.push_back(draw_command{OP_MOVETO, cx, cy, 0.0f, "", 10});
				cmds.push_back(draw_command{OP_LINETO, outer_edge.first, outer_edge.second, 0.0f, "", 10});

				float text_px = outer_edge.first + 15.0f * static_cast<float>(std::cos(theta));
				float text_py = outer_edge.second - 15.0f * static_cast<float>(std::sin(theta));
				cmds.push_back(draw_command{OP_SET_TEXT_COLOR, 0.0f, 0.0f, 0.0f, "#000000", 10});
				cmds.push_back(draw_command{OP_DRAW_TEXT, text_px, text_py, 0.0f, "", 10});
				safe_strncpy(cmds.back().argc1, FormatNumber(180 * theta / M_PI, SCALE_LINEAR).c_str(), sizeof(draw_command::argc1));
			}

			std::pair<float, float> max_edge = ToPolarPixel(0.0, axisY.max);
			float max_pixel_r = std::abs(max_edge.first - cx);
			if (max_pixel_r > 0) {
				cmds.push_back(draw_command{OP_SET_LINE_COLOR, 0.0f, 0.0f, 0.0f, "#000000", 50});
				cmds.push_back(draw_command{OP_SET_LINE_STYLE, 0.0f, 0.0f, 0.0f, "-", 50});
				cmds.push_back(draw_command{OP_SET_LINE_WIDTH, 0.0f, 0.0f, 1.5f, "", 50});
				cmds.push_back(draw_command{OP_DRAW_CIRCLE, cx, cy, max_pixel_r, "", 50});

				std::pair<float, float> xx;

				xx = ToPolarPixel(0, axisY.max);
				cmds.push_back(draw_command{OP_MOVETO, cx, cy, 0.0f, "", 50});
				cmds.push_back(draw_command{OP_LINETO, xx.first, xx.second, 0.0f, "", 50});

				xx = ToPolarPixel(M_PI / 2, axisY.max);
				cmds.push_back(draw_command{OP_MOVETO, cx, cy, 0.0f, "", 50});
				cmds.push_back(draw_command{OP_LINETO, xx.first, xx.second, 0.0f, "", 50});

				xx = ToPolarPixel(M_PI, axisY.max);
				cmds.push_back(draw_command{OP_MOVETO, cx, cy, 0.0f, "", 50});
				cmds.push_back(draw_command{OP_LINETO, xx.first, xx.second, 0.0f, "", 50});

				xx = ToPolarPixel(3 * M_PI / 2, axisY.max);
				cmds.push_back(draw_command{OP_MOVETO, cx, cy, 0.0f, "", 50});
				cmds.push_back(draw_command{OP_LINETO, xx.first, xx.second, 0.0f, "", 50});
			}

		} else if (coord ==  COORD_CARTESIAN) {

			float pxMin = ToPixelX(axisX.min);
			float pxMax = ToPixelX(axisX.max);
			float pyMin = ToPixelY(axisY.min);
			float pyMax = ToPixelY(axisY.max);

			auto xTickValues = axisX.calculateTicks(xTicksCount);
			for (float x : xTickValues) {
				float px = ToPixelX(x);

				cmds.push_back(draw_command{OP_SET_LINE_COLOR, 0.0f, 0.0f, 0.0f, "#C0C0C0", 10});
				cmds.push_back(draw_command{OP_SET_LINE_STYLE, 0.0f, 0.0f, 0.0f, ":", 10});
				cmds.push_back(draw_command{OP_MOVETO, px, pyMin+1, 0.0f, "", 10});
				cmds.push_back(draw_command{OP_LINETO, px, pyMax, 0.0f, "", 10});

				cmds.push_back(draw_command{OP_SET_TEXT_COLOR, 0.0f, 0.0f, 0.0f, "#333333", 10});
				cmds.push_back(draw_command{OP_DRAW_TEXT, px-6.0f, pyMin+15.0f, 0.0f, "", 10});
				safe_strncpy(cmds.back().argc1, FormatNumber(x, axisX.scale).c_str(), sizeof(draw_command::argc1));
			}

			auto yTickValues = axisY.calculateTicks(yTicksCount);
			for (float y : yTickValues) {
				float py = ToPixelY(y);

				cmds.push_back(draw_command{OP_SET_LINE_COLOR, 0.0f, 0.0f, 0.0f, "#C0C0C0", 10});
				cmds.push_back(draw_command{OP_SET_LINE_STYLE, 0.0f, 0.0f, 0.0f, ":", 10});
				cmds.push_back(draw_command{OP_MOVETO, pxMin+1, py, 0.0f, "", 10});
				cmds.push_back(draw_command{OP_LINETO, pxMax, py, 0.0f, "", 10});

				cmds.push_back(draw_command{OP_SET_TEXT_COLOR, 0.0f, 0.0f, 0.0f, "#0000FF", 10});
				cmds.push_back(draw_command{OP_DRAW_TEXT, pxMin-45.0f, py-6.0f, 0.0f, "", 10});
				safe_strncpy(cmds.back().argc1, FormatNumber(y, axisY.scale).c_str(), sizeof(draw_command::argc1));
			}

			if (axisY2.visible) {
				auto y2TickValues = axisY2.calculateTicks(yTicksCount);

				for (float y2 : y2TickValues) {
					float py = ToPixelY(y2, AXIS_SECONDARY);

					cmds.push_back(draw_command{OP_SET_TEXT_COLOR, 0.0f, 0.0f, 0.0f, "#008000", 10});
					cmds.push_back(draw_command{OP_DRAW_TEXT, pxMax+10.0f, py-6.0f, 0.0f, "", 10});
					safe_strncpy(cmds.back().argc1, FormatNumber(y2, axisY2.scale).c_str(), sizeof(draw_command::argc1));
				}
			}

			cmds.push_back(draw_command{OP_SET_LINE_STYLE, 0.0f, 0.0f, 0.0f, "-", 50});
			cmds.push_back(draw_command{OP_SET_LINE_COLOR, 0.0f, 0.0f, 0.0f, "#000000", 50});
			cmds.push_back(draw_command{OP_SET_LINE_WIDTH, 0.0f, 0.0f, 1.5f, "", 50});
			cmds.push_back(draw_command{OP_MOVETO, pxMin, pyMax, 0.0f, "", 50});
			cmds.push_back(draw_command{OP_LINETO, pxMin, pyMin, 0.0f, "", 50});
			cmds.push_back(draw_command{OP_LINETO, pxMax, pyMin, 0.0f, "", 50});
			if (axisY2.visible)
				cmds.push_back(draw_command{OP_LINETO, pxMax, pyMax, 0.0f, "", 50});
		}

		int nret = std::min(cmds.size(), nout);
		for (int i = 0; i < nret; i++) {
			out[i] = cmds.data()[i];
		}
		return nret;
	}
};

#endif // LIBPLOT_HPP
