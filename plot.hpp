#ifndef LIBPLOT_HPP
#define LIBPLOT_HPP
#define _USE_MATH_DEFINES

#include <string>
#include <vector>
#include <map>
#include <algorithm>
#include <cmath>
#include <regex>
#include <cassert>

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

class Plot;
class Curve;

struct Tick {
    float interval = 0.0f; // 0 for auto ticks
    bool visible = true;
    std::string color = "#000000";
};

struct Grid {
    bool visible = true;
    std::string color = "#C0C0C0";
    std::string style = ":";
};

struct Axis {
	Plot *plot = nullptr;

    int scale = SCALE_LINEAR;
    float min = 0.0f;
    float max = 100.0f;
	float ratioMargin = 0.08f;

	float width = 1.0f;
    bool visible = true;
    bool panEnabled = true;
    bool zoomEnabled = true;
    std::string label = "";
    std::string color = "#000000";
    std::string style = "-";
	std::string unit = "";

	bool autorange = true;
	bool autoscroll = false;

    Tick tick;
    Grid grid;

	static constexpr int END_MIN = 0x1;
	static constexpr int END_MAX = 0x2;
	void applyMargin(int ends = END_MIN|END_MAX)
	{
        if (ratioMargin <= 0.0f)
			return;
		if (ends == 0)
			return;

		float d;
        if (scale == SCALE_LOG) {
            float logMin = std::log10(min);
            float logMax = std::log10(max);
			d = (logMax - logMin) * ratioMargin;
			if (ends & END_MIN)
				min = std::pow(10.0, logMin - d);
			if (ends & END_MAX)
				max = std::pow(10.0, logMax + d);
		} else {
			d = (max - min) * ratioMargin;
			if (ends & END_MIN)
				min -= d;
			if (ends & END_MAX)
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

		if (plot && plot->getCoord() == COORD_POLAR && this == &(plot->axisX)) {
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

		if (plot && this == &(plot->axisY2)) {
			std::vector<float> leftTicks = plot->axisY.calculateTicks();

			float leftMin = plot->axisY.min;
			float leftMax = plot->axisY.max;
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

		float length = 0.0f;
		if (plot) {
			if (this == &(plot->axisY) || this == &(plot->axisY2)) {
				length = plot->getClientHeight();
			} else {
				length = plot->getClientWidth();
			}
		}

		int count = (length > 0.0f) ? std::round(length / 70) : 3;
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

class Plot {
private:
	int coord = COORD_CARTESIAN;

	float plotWidth = 400.0f; //pixels
	float plotHeight = 300.0f; //pixels
	float fontWidth = 12.0f; //pixels
	float fontHeight = 15.0f; //pixels

	std::map<std::string, float> paddings = {{"L", 60}, {"T", 40}, {"R", 60}, {"B", 40}}; //pixels

	std::string title = "";
    std::string backgroundColor = ""; // empty for transparent

	std::vector<Curve *> curves;

    void applyMargin() {
        if (coord == COORD_POLAR)
            return;
        axisX.applyMargin();
        axisY.applyMargin();
        if (axisY2.visible)
            axisY2.applyMargin();
    }

public:
	Axis axisX;   // as theta in Polar
	Axis axisY;   // primary (left) Y, as r in Polar
	Axis axisY2;  // secondary (right) Y
	int activeY = AXIS_PRIMARY;

    Plot(int coord = COORD_CARTESIAN) {
		this->coord = coord;

		axisX.plot = this;
        axisY.plot = this;
        axisY2.plot = this;
		axisY2.visible = false;

		if (coord == COORD_POLAR) {
			axisX.min = 0.0f;
			axisX.max = 2 * M_PI;
			axisX.scale = SCALE_LINEAR;
			axisX.panEnabled = false;
			axisX.zoomEnabled = false;
			axisX.autorange = false;

			axisY2.visible = false;
		}

		applyMargin();
    }

    ~Plot() {
        for (Curve *c : curves)
            delete c;
        curves.clear();
    }

	template<typename T = int>
	Curve *addCurve(Curve *curve) {
		curves.push_back(curve);
		autoRange();
		return curve;
	}
	template<typename T = int>
	Curve *removeCurve(Curve *curve) {
		auto it = std::find(curves.begin(), curves.end(), curve);
		if (it != curves.end()) {
			curves.erase(it);
			autoRange();
			return *it;
		}
		return nullptr;
	}
	int getCoord() const { return coord; }
    float getClientWidth() const { return plotWidth - paddings.at("L") - paddings.at("R"); }
    float getClientHeight() const { return plotHeight - paddings.at("T") - paddings.at("B"); }

	template<typename T = int>
	void autoRange(bool force = true) {
		for (auto name: {"X", "Y", "Y2"}) {
			Axis *axis = nullptr;
			if (name == "X")
				axis = &axisX;
			else if (name == "Y")
				axis = &axisY;
			else if (name == "Y2")
				axis = &axisY2;
			if (!axis->autorange)
				continue;
			if (!axis->visible)
				continue;

			std::pair<float, float> minmax{std::numeric_limits<float>::max(),
				                           std::numeric_limits<float>::lowest()};
			for (Curve *curve: curves) {
				if (!curve->visible)
					continue;

				std::pair<float, float> minmax1;
				bool update = false;
				if (name == "X") {
					if (curve->getXData().empty())
						continue;
					minmax1 = curve->getXMinMax();
					update = true;
				} else {
					if (curve->getYData().empty())
						continue;
					minmax1 = curve->getYMinMax();

					if (name == "Y") {
						if (curve->axisY == AXIS_PRIMARY)
							update = true;
					} else if (name == "Y2") {
						if (curve->axisY == AXIS_SECONDARY)
							update = true;
					}
				}

				if (update) {
					if (minmax1.first < minmax.first)
						minmax.first = minmax1.first;
					if (minmax1.second > minmax.second)
						minmax.second = minmax1.second;
				}
			}

			if (minmax.first >= minmax.second)
				continue;

			int ends = 0;
			if (force || axis->min > minmax.first) {
				axis->min = minmax.first;
				ends |= Axis::END_MIN;
			}
			if (force || axis->max < minmax.second) {
				axis->max = minmax.second;
				ends |= Axis::END_MAX;
			}
			axis->applyMargin(ends);
		}
	}

    float toPixelX(float mathX) const {
		if (mathX == axisX.min)
			return paddings.at("L");
		if (mathX == axisX.max)
			return paddings.at("L") + getClientWidth();

		if (!std::isnormal(axisX.max - axisX.min))
			return std::numeric_limits<float>::infinity();

        if (axisX.scale == SCALE_LOG) {
            float logX = std::log10(mathX);
            float logMin = std::log10(axisX.min);
            float logMax = std::log10(axisX.max);

            return paddings.at("L") + (logX - logMin) / (logMax - logMin) * getClientWidth();
        }
        return paddings.at("L") + (mathX - axisX.min) / (axisX.max - axisX.min) * getClientWidth();
    }

    float toPixelY(float mathY, int which = AXIS_PRIMARY) const {
		const Axis& axis = (which == AXIS_SECONDARY) ? axisY2 : axisY;
		if (mathY == axis.min)
			return plotHeight - paddings.at("B");
		if (mathY == axis.max)
			return plotHeight - paddings.at("B") - getClientHeight();

		if (!std::isnormal(axis.max - axis.min))
			return std::numeric_limits<float>::infinity();

        if (axis.scale == SCALE_LOG) {
            float logY = std::log10(mathY);
            float logMin = std::log10(axis.min);
            float logMax = std::log10(axis.max);
            return (plotHeight - paddings.at("B")) - (logY - logMin) / (logMax - logMin) * getClientHeight();
        }
        return (plotHeight - paddings.at("B")) - (mathY - axis.min) / (axis.max - axis.min) * getClientHeight();
    }

    float toMathX(float pixelX) const {
        if (axisX.scale == SCALE_LOG) {
            float logMin = std::log10(axisX.min);
            float logMax = std::log10(axisX.max);
            return std::pow(10.0, logMin + (logMax - logMin) * (pixelX - paddings.at("L")) / getClientWidth());
        }
        return axisX.min + (axisX.max - axisX.min) * (pixelX - paddings.at("L")) / getClientWidth();
    }

    float toMathY(float pixelY, int which = AXIS_PRIMARY) const {
		const Axis& axis = (which == AXIS_SECONDARY) ? axisY2 : axisY;
        if (axis.scale == SCALE_LOG) {
            float logMin = std::log10(axis.min);
            float logMax = std::log10(axis.max);
            return std::pow(10.0, logMin + (logMax - logMin) * (plotHeight - paddings.at("B") - pixelY) / getClientHeight());
        }
        return axis.min + (axis.max - axis.min) * (plotHeight - paddings.at("B") - pixelY) / getClientHeight();
    }

    std::pair<float, float> toPolarPixel(float theta, float r) const {
        float cx = paddings.at("L") + getClientWidth() / 2.0f;
        float cy = paddings.at("T") + getClientHeight() / 2.0f;
        float maxPixelR = std::min(getClientWidth(), getClientHeight()) / 2.0f;

		if (r == 0.0f || r == axisY.min)
			return {cx, cy};
		if (r == axisY.max)
			return {cx + maxPixelR * std::cos(theta),
				    cy - maxPixelR * std::sin(theta)};

		if (!std::isnormal(axisY.max - axisY.min))
			return {std::numeric_limits<float>::infinity(),
				    std::numeric_limits<float>::infinity()};

		float rmin, rmax;
        if (axisY.scale == SCALE_LOG) {
            r = std::log10(r);
            rmin = std::log10(axisY.min);
            rmax = std::log10(axisY.max);
        } else {
			rmin = axisY.min;
			rmax = axisY.max;
        }

        return {cx + (r - rmin) / (rmax - rmin) * maxPixelR * std::cos(theta),
                cy - (r - rmin) / (rmax - rmin) * maxPixelR * std::sin(theta)};
    }

    std::pair<float, float> toPolarMath(float pixelX, float pixelY) const {
        float cx = paddings.at("L") + getClientWidth() / 2.0f;
        float cy = paddings.at("T") + getClientHeight() / 2.0f;
        float maxPixelR = std::min(getClientWidth(), getClientHeight()) / 2.0f;

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

	void pix2math(int pixelX, int pixelY, float *x, float *y, float *y2) const {
        if (coord == COORD_POLAR) {
			auto mathXY = toPolarMath(pixelX, pixelY);
			*x = mathXY.first;
			*y = mathXY.second;
		} else {
			*x = toMathX(pixelX);
			*y = toMathY(pixelY);
			if (axisY2.visible)
				*y2 = toMathY(pixelY, AXIS_SECONDARY);
		}
	}

    void pan(int pixelXDelta, int pixelYDelta)
	{
        if (pixelXDelta == 0 && pixelYDelta == 0)
			return;

        float plotWidth  = getClientWidth();
        float plotHeight = getClientHeight();

        if (pixelXDelta != 0) {
            if (axisX.scale == SCALE_LOG) {
                axisX.pan(-(std::log10(axisX.max) - std::log10(axisX.min)) * pixelXDelta / plotWidth);
            } else {
                axisX.pan(-(toMathX(pixelXDelta) - toMathX(0.0f)));
            }
        }

        if (pixelYDelta != 0) {
            if (axisY.scale == SCALE_LOG) {
                if (coord == COORD_POLAR) {
					axisY.pan(std::log10(toPolarMath(pixelXDelta, pixelYDelta).second) - std::log10(toPolarMath(0.0f, 0.0f).second));
                } else {
					axisY.pan((std::log10(axisY.max) - std::log10(axisY.min)) * pixelYDelta / plotHeight);
                }
            } else {
                if (coord == COORD_POLAR) {
                    axisY.pan(toPolarMath(pixelXDelta, pixelYDelta).second - toPolarMath(0.0f, 0.0f).second);
                } else {
                    axisY.pan(-(toMathY(pixelYDelta) - toMathY(0.0f)));
                }
            }

			if (axisY2.visible) {
				if (axisY2.scale == SCALE_LOG) {
					axisY2.pan((std::log10(axisY2.max) - std::log10(axisY2.min)) * pixelYDelta / plotHeight);
				} else {
					axisY2.pan(-(toMathY(pixelYDelta, AXIS_SECONDARY) - toMathY(0.0f, AXIS_SECONDARY)));
				}
			}
        }
    }

    void zoom(int pixelX, int pixelY, float factor)
	{
		axisX.zoom(toMathX(pixelX), factor);
		if (coord == COORD_POLAR)
			axisY.zoom(axisY.min, factor); // only zoom in and out around the center
		else
			axisY.zoom(toMathY(pixelY), factor);
		if (axisY2.visible)
			axisY2.zoom(toMathY(pixelY, AXIS_SECONDARY), factor);
    }


    int set_option(const std::string& option, const std::string& value) {
		if (option == "plot.size") {
			int w, h;
			if (sscanf(value.c_str(), "%dx%d", &w, &h) == 2) {
				plotWidth = w;
				plotHeight = h;
				return 0;
			}
		} else if (option == "plot.title") {
			title = value;
		} else if (option == "plot.background.color") {
			backgroundColor = value;
		} else if (option == "plot.font.size") {
			int w, h;
			if (sscanf(value.c_str(), "%dx%d", &w, &h) == 2) {
				fontWidth = w;
				fontHeight = h;
				return 0;
			}
		} else if (std::regex_match(option, std::regex(R"(^plot\.axis(X|Y|Y2)\..+$)"))) {
			Axis *axis = nullptr;
			std::smatch matches;
			if (std::regex_match(option, matches, std::regex(R"(^plot\.axis(X|Y|Y2)\.(.+)$)"))) {
				if (matches[1] == "X")
					axis = &axisX;
				else if (matches[1] == "Y")
					axis = &axisY;
				else if (matches[1] == "Y2")
					axis = &axisY2;
				else
					assert(false && "Must be X, Y or Y2");

				if (axis) {
					std::string suffix = matches[2];

					if (suffix == "active") {
						if (coord == COORD_CARTESIAN) {
							if (matches[1] == "Y")
								activeY = AXIS_PRIMARY;
							else if (matches[1] == "Y2")
								activeY = AXIS_SECONDARY;
						}
					} else if (suffix == "autorange") {
						if (!(coord == COORD_POLAR && axis == &axisX)) {
							if (value == "true")
								axis->autorange = true;
							else if (value == "false")
								axis->autorange = false;
						}
					} else if (suffix == "autoscroll") {
						if (!(coord == COORD_POLAR && axis == &axisX)) {
							if (value == "true")
								axis->autoscroll = true;
							else if (value == "false")
								axis->autoscroll = false;
						}
					} else if (suffix == "range") {
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
					} else if (suffix == "label") {
						axis->label = value;
					} else if (suffix == "unit") {
						axis->unit = value;
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
					}
				}
			}
		}
		return -1;
	}

	int render(struct plt_render_cmd *out, size_t nout) const;
};

struct Curve {
	Plot *plot = nullptr;
	int axisY = AXIS_PRIMARY;

	bool visible = true;
    float width = 2.0f; // pixels
    std::string label = "";
    std::string color = "#000000";
    std::string style = "-";

	std::string markerShape = "";
	float markerSize = 6.0f; // pixels
	std::string markerColor = "#000000";

    Curve(Plot *plot) {
		this->plot = plot;

		axisY = plot->activeY;
		plot->addCurve(this);
	}
    ~Curve() {
		plot->removeCurve(this);
	}
	const std::vector<float> &getXData() const { return xData; }
	const std::vector<float> &getYData() const { return yData; }
	std::pair<float, float> getXMinMax() const { return {xmin, xmax}; }
	std::pair<float, float> getYMinMax() const { return {ymin, ymax}; }

	void updateData(const std::vector<float> &x, const std::vector<float> &y) {
		xData.assign(x.begin(), x.end());
		yData.assign(y.begin(), y.end());

		if (!x.empty()) {
			auto [min_it, max_it] = std::minmax_element(x.begin(), x.end());
			xmin = *min_it;
			xmax = *max_it;
		}
		if (!y.empty()) {
			auto [min_it, max_it] = std::minmax_element(y.begin(), y.end());
			ymin = *min_it;
			ymax = *max_it;
		}

		plot->autoRange();
	}
	void appendData(const std::vector<float> &x, const std::vector<float> &y) {
		xData.insert(xData.end(), x.begin(), x.end());
		yData.insert(yData.end(), y.begin(), y.end());

		if (!x.empty()) {
			auto [min_it, max_it] = std::minmax_element(x.begin(), x.end());
			if (xmin > *min_it)
				xmin = *min_it;
			if (xmax < *max_it)
				xmax = *max_it;

			Axis *axis = &plot->axisX;
			if (axis->autoscroll) {
				float delta = 0;

				if (xmax > axis->max)
					delta = (xmax - axis->max) + (axis->max - axis->min) / 2;
				else if (xmin < axis->min)
					delta = -((axis->min - xmin) + (axis->max - axis->min) / 2);

				axis->pan(delta);
			}
		}
		if (!y.empty()) {
			auto [min_it, max_it] = std::minmax_element(y.begin(), y.end());
			if (ymin > *min_it)
				ymin = *min_it;
			if (ymax < *max_it)
				ymax = *max_it;

			Axis *axis = nullptr;
			if (axisY == AXIS_PRIMARY)
				axis = &plot->axisY;
			else
				axis = &plot->axisY2;
			if (axis->autoscroll) {
				float delta = 0;

				if (ymax > axis->max)
					delta = (ymax - axis->max) + (axis->max - axis->min) / 2;
				else if (ymin < axis->min)
					delta = -((axis->min - ymin) + (axis->max - axis->min) / 2);

				axis->pan(delta);
			}
		}
	}
    int set_option(const std::string& option, const std::string& value) {
		if (option == "curve.visible") {
			if (value == "true")
				visible = true;
			else if (value == "false")
				visible = false;
		} else if (option == "curve.color") {
			color = value;
		} else if (option == "curve.style") {
			style = value;
		} else if (option == "curve.width") {
			float a;
			if ((std::sscanf(value.c_str(), "%f", &a) == 1) && (a > 0)) {
				width = a;
			}
		} else if (option == "curve.label") {
			label = value;
		} else if (option == "curve.marker.shape") {
			markerShape = value;
		} else if (option == "curve.marker.size") {
			float a;
			if ((std::sscanf(value.c_str(), "%f", &a) == 1) && (a > 0)) {
				markerSize = a;
			}
		} else if (option == "curve.marker.color") {
			markerColor = value;
		}

		return 0;
	}

private:
    std::vector<float> xData;
    std::vector<float> yData;

	// cache
	float xmin, xmax, ymin, ymax;
};

#endif // LIBPLOT_HPP
