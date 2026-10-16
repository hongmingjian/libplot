#ifndef LIBPLOT_HPP
#define LIBPLOT_HPP
#define _USE_MATH_DEFINES

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>
#include <regex>

#include "libplot.h"

static float NiceNumber(float x, bool round)
{
	int exp = static_cast<int>(std::floor(std::log10(x)));
	float f = x / std::pow(10.0, exp);
	float nf;
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

class Figure;

enum MarkerType {
    MARKER_NONE = 0,
    MARKER_X,        // 'x'
    MARKER_PLUS,     // '+'
    MARKER_CIRCLE,   // 'o'
    MARKER_SQUARE,   // 's'
    MARKER_TRIANGLE, // '^'
    MARKER_DIAMOND   // 'd'
};

struct Tick {
    float interval = 0.0f; // 0 for auto ticks
    bool visible = true;
    std::string color = "#000000";
    std::string font; // "Arial,12"
};

struct Grid {
    bool visible = true;
    std::string color = "#E0E0E0";
    std::string style = ":";
};

struct Axis {
	Figure *figure = nullptr;

	//std::string name = "";
    std::string label = "";

    int scale = SCALE_LINEAR;
    float min = 0.0f;
    float max = 100.0f;
	float ratioMargin = 0.08f;

    bool visible = true;
	float width = 1.0f;
    std::string color = "#000000";
    std::string style = "-";

    Tick tick;
    Grid grid;

    bool panEnabled = true;
    bool zoomEnabled = true;

	void applyMargin()
	{
        if (ratioMargin <= 0.0f)
			return;

		float d;
        if (scale == SCALE_LOG) {
            float logMin = std::log10(min);
            float logMax = std::log10(max);
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
		if (max <= min || !std::isnormal(max - min))
			return;

		float newMin = min;
		float newMax = max;

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

		if (factor < 1) {
			float magnitude = std::max(std::abs(newMin), std::abs(newMax));
			if (magnitude == 0.0f)
				magnitude = 1.0f;
			if ((newMax - newMin) / magnitude < 10.0f * std::numeric_limits<float>::epsilon())
				return;
		}

		min = newMin;
		max = newMax;
	}

	template<typename T = int>
	std::vector<float> calculateTicks() const
	{
		std::vector<float> ticks;

		if (figure && figure->coord == COORD_POLAR && this == &(figure->axisX)) {
			float interval = tick.interval;
			while (interval < 0)
				interval += 2 * M_PI;
			while (interval > 2 * M_PI)
				interval -= 2 * M_PI;

			int count = (interval > 0.0f) ? std::round(2 * M_PI / interval) : 8;
			if (count < 4)
				count = 4;
			for (int i = 0; i < count; ++i)
				ticks.push_back(2 * M_PI * i / count);
			return ticks;
		}

		if (figure && this == &(figure->axisY2)) {
			std::vector<float> leftTicks = figure->axisY.calculateTicks();

			float leftMin = figure->axisY.min;
			float leftMax = figure->axisY.max;
			float leftRange = leftMax - leftMin;

			if (std::isnormal(leftRange)) {
				if (scale == SCALE_LOG) {
					float logMin2 = std::log10(min);
					float logMax2 = std::log10(max);
					for (float leftVal : leftTicks)
						ticks.push_back(std::pow(10.0, logMin2 + (logMax2 - logMin2) * (leftVal - leftMin) / leftRange));
				} else {
					for (float leftVal : leftTicks)
						ticks.push_back(min + (max - min) * (leftVal - leftMin) / leftRange);
				}
			}
			return ticks;
		}

		float plotLength = 0.0f;
		if (figure) {
			if (this == &(figure->axisY) || this == &(figure->axisY2)) {
				plotLength = figure->GetPlotHeight();
			} else {
				plotLength = figure->GetPlotWidth();
			}
		}

		int count = (plotLength > 0.0f) ? std::round(plotLength / 70) : 3;
		if (count < 3)
			count = 3;

		if (scale == SCALE_LOG) {
			float logMin = std::log10(min);
			float logMax = std::log10(max);
			float decadeRange = logMax - logMin;

			if (decadeRange >= 1.0) {
				int startExponent = (int)std::floor(logMin);
				int endExponent   = (int)std::ceil(logMax);
				int exponentStep = 1;
				if (tick.interval > 0.0f) {
					exponentStep = static_cast<int>(std::round(tick.interval));
					if (exponentStep < 1)
						exponentStep = 1;
				} else {
					if (endExponent - startExponent > count && count > 1) {
						exponentStep = (endExponent - startExponent) / (count - 1);
						if (exponentStep < 1)
							exponentStep = 1;
					}
				}
				float displayMin = std::pow(10.0, startExponent);
				float displayMax = std::pow(10.0, endExponent);
				for (int expv = startExponent; expv <= endExponent; expv += exponentStep) {
					float val = std::pow(10.0, expv);
					if (val >= displayMin - 1e-6 && val <= displayMax + 1e-6) {
						if (val >= min && val <= max)
							ticks.push_back((float)val);
					}
				}
			}
			else {
				float niceStep;
				if (tick.interval > 0.0f) {
					niceStep = tick.interval;
				} else {
					float baseRange = NiceNumber(max - min, false);
					niceStep = NiceNumber(baseRange / (count - 1), true);
				}
				float displayMin = std::floor(min / niceStep) * niceStep;
				float displayMax = std::ceil(max / niceStep) * niceStep;
				for (float v = displayMin; v <= displayMax + (niceStep * 0.0001); v += niceStep) {
					if (v >= min - (niceStep * 0.0001) && v <= max + (niceStep * 0.0001)) {
						ticks.push_back((float)v);
					}
				}
			}
			return ticks;
		}

		float niceStep;
		if (tick.interval > 0.0f) {
			niceStep = tick.interval;
		} else {
			float range = NiceNumber(max - min, false);
			niceStep = NiceNumber(range / (count - 1), true);
		}

		float start = std::floor(min / niceStep) * niceStep;
		float end   = std::ceil(max / niceStep) * niceStep;
		for (float v = start; v <= end + (niceStep * 0.0001); v += niceStep) {
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
private:
	float width = 400.0f; //in pixels
	float height = 300.0f; //ditto
	std::map<std::string, float> paddings = {{"L", 60}, {"T", 40}, {"R", 60}, {"B", 40}}; //ditto

    void applyMargin() {
        if (coord == COORD_POLAR)
            return;
        axisX.applyMargin();
        axisY.applyMargin();
        if (axisY2.visible)
            axisY2.applyMargin();
    }

public:
	int coord = COORD_CARTESIAN;

	Axis axisX;   // as theta in Polar
	Axis axisY;   // primary (left) Y, as r in Polar
	Axis axisY2;  // secondary (right) Y

	std::vector<Curve *> curves;

    Figure(int coord_ = COORD_CARTESIAN) {
		coord = coord_;

		axisX.figure = this;
        axisY.figure = this;
        axisY2.figure = this;
		axisY2.visible = false;

		if (coord == COORD_POLAR) {
			axisX.min = 0.0f;
			axisX.max = 2 * M_PI;
			axisX.scale = SCALE_LINEAR;
			axisX.panEnabled = false;
			axisX.zoomEnabled = false;

			axisY2.visible = false;
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
            float logX = std::log10(mathX);
            float logMin = std::log10(axisX.min);
            float logMax = std::log10(axisX.max);

            return paddings.at("L") + GetPlotWidth() * (logX - logMin) / (logMax - logMin);
        }
        return paddings.at("L") + GetPlotWidth() * (mathX - axisX.min) / (axisX.max - axisX.min);
    }

    float ToPixelY(float mathY, int which = AXIS_PRIMARY) const {
		const Axis& axis = (which == AXIS_SECONDARY) ? axisY2 : axisY;

		if (!std::isnormal(axis.max - axis.min))
			return std::numeric_limits<float>::quiet_NaN();

        if (axis.scale == SCALE_LOG) {
            float logY = std::log10(mathY);
            float logMin = std::log10(axis.min);
            float logMax = std::log10(axis.max);
            return (height - paddings.at("B")) - GetPlotHeight() * (logY - logMin) / (logMax - logMin);
        }
        return (height - paddings.at("B")) - GetPlotHeight() * (mathY - axis.min) / (axis.max - axis.min);
    }

    float ToMathX(float pixelX) const {
        if (axisX.scale == SCALE_LOG) {
            float logMin = std::log10(axisX.min);
            float logMax = std::log10(axisX.max);
            return std::pow(10.0, logMin + (logMax - logMin) * (pixelX - paddings.at("L")) / GetPlotWidth());
        }
        return axisX.min + (axisX.max - axisX.min) * (pixelX - paddings.at("L")) / GetPlotWidth();
    }

    float ToMathY(float pixelY, int which = AXIS_PRIMARY) const {
		const Axis& axis = (which == AXIS_SECONDARY) ? axisY2 : axisY;
        if (axis.scale == SCALE_LOG) {
            float logMin = std::log10(axis.min);
            float logMax = std::log10(axis.max);
            return std::pow(10.0, logMin + (logMax - logMin) * (height - paddings.at("B") - pixelY) / GetPlotHeight());
        }
        return axis.min + (axis.max - axis.min) * (height - paddings.at("B") - pixelY) / GetPlotHeight();
    }

    std::pair<float, float> ToPolarPixel(float theta, float r) const {
        float cx = paddings.at("L") + GetPlotWidth() / 2.0f;
        float cy = paddings.at("T") + GetPlotHeight() / 2.0f;

		if (r == 0.0f)
			return {cx, cy};

		if (!std::isnormal(axisY.max - axisY.min))
			return {std::numeric_limits<float>::quiet_NaN(),
				    std::numeric_limits<float>::quiet_NaN()};

		float rmin, rmax;
        if (axisY.scale == SCALE_LOG) {
            r = std::log10(r);
            rmin = std::log10(axisY.min);
            rmax = std::log10(axisY.max);
        } else {
			rmin = axisY.min;
			rmax = axisY.max;
        }

        float maxPixelR = std::min(GetPlotWidth(), GetPlotHeight()) / 2.0f;
        return {cx + maxPixelR * (r - rmin) / (rmax - rmin) * std::cos(theta),
                cy - maxPixelR * (r - rmin) / (rmax - rmin) * std::sin(theta)};
    }

    std::pair<float, float> ToPolarMath(float pixelX, float pixelY) const {
        float cx = paddings.at("L") + GetPlotWidth() / 2.0f;
        float cy = paddings.at("T") + GetPlotHeight() / 2.0f;
        float maxPixelR = std::min(GetPlotWidth(), GetPlotHeight()) / 2.0f;

        float dx = pixelX - cx;
        float dy = cy - pixelY;
        float theta = std::atan2(dy, dx);
        if (theta < 0)
            theta += 2.0 * M_PI;
        float r;

        if (axisY.scale == SCALE_LOG) {
            float logMin = std::log10(axisY.min);
            float logMax = std::log10(axisY.max);

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

        float plotWidth  = GetPlotWidth();
        float plotHeight = GetPlotHeight();

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


    int set_option(const std::string& option, const std::string& value) {
		if (option == "figure.size") {
			int w, h;
			if (sscanf(value.c_str(), "%dx%d", &w, &h) == 2) {
				width = w;
				height = h;
				return 0;
			}
		} else if (std::regex_match(option, std::regex(R"(^figure\.axis(X|Y|Y2)\..+$)"))) {
			Axis *axis = nullptr;
			std::smatch matches;
			if (std::regex_match(option, matches, std::regex(R"(^figure\.axis(X|Y|Y2)\.(.+)$)"))) {
				if (matches[1] == "X")
					axis = &axisX;
				else if (matches[1] == "Y")
					axis = &axisY;
				else if (matches[1] == "Y2")
					axis = &axisY2;

				if (axis) {
					std::string suffix = matches[2];

					if (suffix == "range") {
						if (!(coord == COORD_POLAR && axis == &axisX)) {
							float a, b;
							if (std::sscanf(value.c_str(), "%f , %f", &a, &b) == 2) {
								axis->min = a;
								axis->max = b;
							}
						}
					} else if (suffix == "scale") {
						if (!(coord == COORD_POLAR && axis == &axisX)) {
							if (value == "linear")
								axis->scale = SCALE_LINEAR;
							else if (value == "log")
								axis->scale = SCALE_LOG;
						}
					} else if (suffix == "visible") {
						if (!(coord == COORD_POLAR && axis == &axisY2)) {
							if (value == "true")
								axis->visible = true;
							else if (value == "false")
								axis->visible = false;
						}
					} else if (suffix == "pan") {
						if (!(coord == COORD_POLAR && axis == &axisX)) {
							if (value == "enabled")
								axis->panEnabled = true;
							else if (value == "disabled")
								axis->panEnabled = false;
						}
					} else if (suffix == "zoom") {
						if (!(coord == COORD_POLAR && axis == &axisX)) {
							if (value == "enabled")
								axis->zoomEnabled = true;
							else if (value == "disabled")
								axis->zoomEnabled = false;
						}
					} else if (suffix == "width") {
						float a;
						if (std::sscanf(value.c_str(), "%f", &a) == 1) {
							axis->width = a;
						}
					} else if (suffix == "color") {
						axis->color = value;
					} else if (suffix == "style") {
						axis->style = value;
					} else if (suffix == "grid.visible") {
						if (value == "true")
							axis->grid.visible = true;
						else if (value == "false")
							axis->grid.visible = false;
					} else if (suffix == "grid.color") {
						axis->grid.color = value;
					} else if (suffix == "grid.style") {
						axis->grid.style = value;
					} else if (suffix == "tick.visible") {
						if (value == "true")
							axis->tick.visible = true;
						else if (value == "false")
							axis->tick.visible = false;
					} else if (suffix == "tick.interval") {
						float a;
						if (std::sscanf(value.c_str(), "%f", &a) == 1) {
							if (!(coord == COORD_POLAR && axis == &axisX)) {
								axis->tick.interval = a;
							} else {
								axis->tick.interval = a * M_PI / 180;
							}
						}
					} else if (suffix == "tick.color") {
						axis->tick.color = value;
					} else if (suffix == "tick.font") {
						axis->tick.font = value;
					}
				}
			}
		}
		return -1;
	}

	int show(struct draw_command *out, size_t nout) const;
};

#endif // LIBPLOT_HPP
