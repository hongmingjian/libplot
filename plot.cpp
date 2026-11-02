#include "plot.hpp"

#include <sstream>
#include <iomanip>

template<typename T>
static T NiceNumber(T x, bool round)
{
	int exp = static_cast<int>(std::floor(std::log10(x)));
	T f = x / std::pow(10.0, exp);
	T nf;
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

template<typename T>
static std::string FormatNumber(T val, int scale, std::string unit = "")
{
    std::ostringstream oss;

    if (unit.empty()) {
        if (scale == SCALE_LOG && (val >= 1000.0f || val <= 0.001f)) {
            oss << std::scientific << std::setprecision(0) << val;
            return oss.str();
        }
        oss << std::setprecision(4) << val;
        return oss.str();
    }

    if (val == 0.0f) {
        oss << std::setprecision(4) << 0.0f << " " << unit;
        return oss.str();
    }

    if (scale == SCALE_LOG && (val >= 1000.0f || val <= 0.001f)) {
        oss << std::scientific << std::setprecision(0) << val << " " << unit;
        return oss.str();
    }

    static const char* prefixes[] = { "p", "u", "m", "", "k", "M", "G", "T" };

    T scaled_val = val;
    int index = 3; // "" (10^0)

    while (std::abs(scaled_val) >= 1000.0f && index < 7) {
        scaled_val /= 1000.0f;
        index++;
    }
    while (std::abs(scaled_val) < 1.0f && std::abs(scaled_val) > 0.0f && index > 0) {
        scaled_val *= 1000.0f;
        index--;
    }

    oss << std::setprecision(4) << scaled_val << " " << prefixes[index] << unit;
    return oss.str();
}

template<typename T>
void Axis<T>::applyMargin(int ends)
{
	if (ratioMargin <= 0.0f)
		return;
	if (ends == 0)
		return;

	T d;
	if (scale == SCALE_LOG) {
		T logMin = std::log10(min);
		T logMax = std::log10(max);
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

template<typename T>
void Axis<T>::pan(T d)
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

template<typename T>
void Axis<T>::zoom(T center, float factor)
{
	if (!zoomEnabled)
		return;
	if (factor <= 0)
		return;
	if (max <= min || !std::isnormal(max - min))
		return;

	T newMin = min;
	T newMax = max;

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
		T magnitude = std::max(std::abs(newMin), std::abs(newMax));
		if (magnitude == 0.0f)
			magnitude = 1.0f;
		if ((newMax - newMin) / magnitude < 10.0f * std::numeric_limits<T>::epsilon())
			return;
	}

	min = newMin;
	max = newMax;
}

template<typename T>
std::vector<T> Axis<T>::calculateTicks() const
{
	std::vector<T> ticks;

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
		std::vector<T> leftTicks = plot->axisY.calculateTicks();

		T leftMin = plot->axisY.min;
		T leftMax = plot->axisY.max;
		T leftRange = leftMax - leftMin;

		if (std::isnormal(leftRange)) {
			if (scale == SCALE_LOG) {
				T logMin2 = std::log10(min);
				T logMax2 = std::log10(max);
				for (T leftVal : leftTicks)
					ticks.push_back(std::pow(10.0, logMin2 + (logMax2 - logMin2) * (leftVal - leftMin) / leftRange));
			} else {
				for (T leftVal : leftTicks)
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
		T logMin = std::log10(min);
		T logMax = std::log10(max);
		T decadeRange = logMax - logMin;

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
			T displayMin = std::pow(10.0, startExponent);
			T displayMax = std::pow(10.0, endExponent);
			for (int expv = startExponent; expv <= endExponent; expv += exponentStep) {
				T val = std::pow(10.0, expv);
				if (val >= displayMin - 1e-6 && val <= displayMax + 1e-6) {
					if (val >= min && val <= max)
						ticks.push_back(val);
				}
			}
		}
		else {
			T niceStep;
			if (tick.interval > 0.0f) {
				niceStep = tick.interval;
			} else {
				T baseRange = NiceNumber(max - min, false);
				niceStep = NiceNumber(baseRange / (count - 1), true);
			}
			T displayMin = std::floor(min / niceStep) * niceStep;
			T displayMax = std::ceil(max / niceStep) * niceStep;
			for (T v = displayMin; v <= displayMax + (niceStep * 0.0001); v += niceStep) {
				if (v >= min - (niceStep * 0.0001) && v <= max + (niceStep * 0.0001)) {
					ticks.push_back(v);
				}
			}
		}
		return ticks;
	}

	T niceStep;
	if (tick.interval > 0.0f) {
		niceStep = tick.interval;
	} else {
		T range = NiceNumber(max - min, false);
		niceStep = NiceNumber(range / (count - 1), true);
	}

	T start = std::floor(min / niceStep) * niceStep;
	T end   = std::ceil(max / niceStep) * niceStep;
	for (T v = start; v <= end + (niceStep * 0.0001); v += niceStep) {
		if (v >= min - (niceStep * 0.0001) && v <= max + (niceStep * 0.0001)) {
			ticks.push_back(v);
		}
	}

	return ticks;
}

template<typename T>
Plot<T>::Plot(int coord)
{
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

template<typename T>
Plot<T>::~Plot()
{
	for (Curve<T> *c : curves)
		delete c;
	curves.clear();
}

template<typename T>
Curve<T> *Plot<T>::addCurve(Curve<T> *curve)
{
	curves.push_back(curve);
	autoRange();
	return curve;
}

template<typename T>
Curve<T> *Plot<T>::removeCurve(Curve<T> *curve)
{
	auto it = std::find(curves.begin(), curves.end(), curve);
	if (it != curves.end()) {
		Curve<T> *curve = *it;
		curves.erase(it);
		autoRange();
		return curve;
	}
	return nullptr;
}

template<typename T>
void Plot<T>::applyMargin()
{
	if (coord == COORD_POLAR)
		return;
	axisX.applyMargin();
	axisY.applyMargin();
	if (axisY2.visible)
		axisY2.applyMargin();
}

template<typename T>
void Plot<T>::autoRange(bool force)
{
	for (const std::string &name: {"X", "Y", "Y2"}) {
		Axis<T> *axis = nullptr;
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

		std::pair<T, T> minmax{std::numeric_limits<T>::max(),
			                   std::numeric_limits<T>::lowest()};
		for (Curve<T> *curve: curves) {
			if (!curve->visible)
				continue;

			std::pair<T, T> minmax1;
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
			ends |= Axis<T>::END_MIN;
		}
		if (force || axis->max < minmax.second) {
			axis->max = minmax.second;
			ends |= Axis<T>::END_MAX;
		}
		axis->applyMargin(ends);
	}
}

template<typename T>
float Plot<T>::toPixelX(T mathX) const
{
	if (mathX == axisX.min)
		return paddings.at("L");
	if (mathX == axisX.max)
		return paddings.at("L") + getClientWidth();

	if (!std::isnormal(axisX.max - axisX.min))
		return std::numeric_limits<float>::infinity();

	if (axisX.scale == SCALE_LOG) {
		T logX = std::log10(mathX);
		T logMin = std::log10(axisX.min);
		T logMax = std::log10(axisX.max);

		return paddings.at("L") + (logX - logMin) / (logMax - logMin) * getClientWidth();
	}
	return paddings.at("L") + (mathX - axisX.min) / (axisX.max - axisX.min) * getClientWidth();
}

template<typename T>
float Plot<T>::toPixelY(T mathY, int which) const
{
	const Axis<T>& axis = (which == AXIS_SECONDARY) ? axisY2 : axisY;
	if (mathY == axis.min)
		return plotHeight - paddings.at("B");
	if (mathY == axis.max)
		return plotHeight - paddings.at("B") - getClientHeight();

	if (!std::isnormal(axis.max - axis.min))
		return std::numeric_limits<float>::infinity();

	if (axis.scale == SCALE_LOG) {
		T logY = std::log10(mathY);
		T logMin = std::log10(axis.min);
		T logMax = std::log10(axis.max);
		return (plotHeight - paddings.at("B")) - (logY - logMin) / (logMax - logMin) * getClientHeight();
	}
	return (plotHeight - paddings.at("B")) - (mathY - axis.min) / (axis.max - axis.min) * getClientHeight();
}

template<typename T>
T Plot<T>::toMathX(float pixelX) const
{
	if (axisX.scale == SCALE_LOG) {
		T logMin = std::log10(axisX.min);
		T logMax = std::log10(axisX.max);
		return std::pow(10.0, logMin + (logMax - logMin) * (pixelX - paddings.at("L")) / getClientWidth());
	}
	return axisX.min + (axisX.max - axisX.min) * (pixelX - paddings.at("L")) / getClientWidth();
}

template<typename T>
T Plot<T>::toMathY(float pixelY, int which) const
{
	const Axis<T>& axis = (which == AXIS_SECONDARY) ? axisY2 : axisY;
	if (axis.scale == SCALE_LOG) {
		T logMin = std::log10(axis.min);
		T logMax = std::log10(axis.max);
		return std::pow(10.0, logMin + (logMax - logMin) * (plotHeight - paddings.at("B") - pixelY) / getClientHeight());
	}
	return axis.min + (axis.max - axis.min) * (plotHeight - paddings.at("B") - pixelY) / getClientHeight();
}

template<typename T>
std::pair<float, float> Plot<T>::toPolarPixel(T theta, T r) const
{
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

	T rmin, rmax;
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

template<typename T>
std::pair<T, T> Plot<T>::toPolarMath(float pixelX, float pixelY) const
{
	float cx = paddings.at("L") + getClientWidth() / 2.0f;
	float cy = paddings.at("T") + getClientHeight() / 2.0f;
	float maxPixelR = std::min(getClientWidth(), getClientHeight()) / 2.0f;

	float dx = pixelX - cx;
	float dy = cy - pixelY;
	T theta = std::atan2(dy, dx);
	if (theta < 0)
		theta += 2.0 * M_PI;
	T r;

	if (axisY.scale == SCALE_LOG) {
		T logMin = std::log10(axisY.min);
		T logMax = std::log10(axisY.max);

		r = std::pow(10.0, logMin + (logMax - logMin) * std::sqrt(dx * dx + dy * dy) / maxPixelR);
	} else {
		r = axisY.min + (axisY.max - axisY.min) * std::sqrt(dx * dx + dy * dy) / maxPixelR;
	}

	return {theta, r};
}

template<typename T>
void Plot<T>::pix2math(float pixelX, float pixelY, T *x, T *y, T *y2) const
{
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

template<typename T>
void Plot<T>::pan(float pixelXDelta, float pixelYDelta)
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

template<typename T>
void Plot<T>::zoom(float pixelX, float pixelY, float factor)
{
	axisX.zoom(toMathX(pixelX), factor);
	if (coord == COORD_POLAR)
		axisY.zoom(axisY.min, factor); // only zoom in and out around the center
	else
		axisY.zoom(toMathY(pixelY), factor);
	if (axisY2.visible)
		axisY2.zoom(toMathY(pixelY, AXIS_SECONDARY), factor);
}

template<typename T>
int Plot<T>::set_option(const std::string& option, const std::string& value)
{
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
		Axis<T> *axis = nullptr;
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
						T a, b;
						std::string fmt;
						if constexpr (std::is_same_v<T, float>) {
							fmt = "%f , %f";
						} else if constexpr (std::is_same_v<T, double>) {
							fmt = "%lf , %lf";
						}
						if (std::sscanf(value.c_str(), fmt.c_str(), &a, &b) == 2) {
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

template <typename T>
using Point = std::pair<T, T>; // first: x, second: y

template <typename T>
using Rect = std::pair<Point<T>, Point<T>>; // first: (left, top), second: (right, bottom)

template <typename T>
inline T crossProduct(const Point<T>& p0, const Point<T>& p1, const Point<T>& p2)
{
    return (p1.first - p0.first) * (p2.second - p0.second) - (p1.second - p0.second) * (p2.first - p0.first);
}

template <typename T>
inline bool checkSegmentIntersect(const Point<T>& p1, const Point<T>& p2, const Point<T>& p3, const Point<T>& p4)
{
    if (std::max(p1.first, p2.first) < std::min(p3.first, p4.first) ||
        std::max(p3.first, p4.first) < std::min(p1.first, p2.first) ||
        std::max(p1.second, p2.second) < std::min(p3.second, p4.second) ||
        std::max(p3.second, p4.second) < std::min(p1.second, p2.second)) {
        return false;
    }

    T cp1 = crossProduct(p1, p2, p3);
    T cp2 = crossProduct(p1, p2, p4);
    T cp3 = crossProduct(p3, p4, p1);
    T cp4 = crossProduct(p3, p4, p2);

    return (cp1 * cp2 <= 0) && (cp3 * cp4 <= 0);
}

inline struct plt_render_cmd plt_render_cmd_(
		unsigned int op, unsigned int zIndex,
		float x, float y, float arg1, float arg2, const std::string& arg3)
{
    struct plt_render_cmd cmd{op, zIndex, 0, x, y, arg1, arg2, ""};
    strncpy_s(cmd.arg3, sizeof(cmd.arg3), arg3.c_str(), _TRUNCATE);
    return cmd;
}

template<typename T>
int Plot<T>::render(struct plt_render_cmd *out, size_t nout) const
{
	Rect<float> rect;
	std::vector<struct plt_render_cmd> cmds;

	if (coord == COORD_POLAR) {
		auto [cx, cy] = toPolarPixel(0.0, 0.0);

		std::pair<float, float> max_edge = toPolarPixel(0.0, axisY.max);
		float max_pixel_r = std::abs(max_edge.first - cx);
		if (max_pixel_r > 0) {
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 5, 0.0f, 0.0f, 0.0f, 0.0f, ""));
			cmds.push_back(plt_render_cmd_(OP_DRAW_ELLIPSE, 5, cx, cy, max_pixel_r, max_pixel_r, backgroundColor));
		}

		cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 10, 0.0f, 0.0f, 1.0f, 0.0f, ""));

		cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 10, 0.0f, 0.0f, 0.0f, 0.0f, axisX.grid.color));
		cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 10, 0.0f, 0.0f, 0.0f, 0.0f, axisX.grid.style));
		cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 10, 0.0f, 0.0f, 0.0f, 0.0f, axisX.tick.color));
		for (T theta : axisX.calculateTicks()) {
			if (theta <= axisX.min)
				continue;
			std::pair<float, float> outer_edge = toPolarPixel(theta, axisY.max);

			if (axisX.grid.visible) {
				cmds.push_back(plt_render_cmd_(OP_MOVETO, 10, cx, cy, 0.0f, 0.0f, ""));
				cmds.push_back(plt_render_cmd_(OP_LINETO, 10, outer_edge.first, outer_edge.second, 0.0f, 0.0f, ""));
			}

			if (axisX.tick.visible) {
				cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 10, outer_edge.first, outer_edge.second, 0,
							((abs(theta-M_PI_2)<std::numeric_limits<T>::epsilon()||abs(theta-3*M_PI_2)<std::numeric_limits<T>::epsilon())?ALIGN_HCENTER:((theta>M_PI_2&&theta<3*M_PI_2)?ALIGN_RIGHT:ALIGN_LEFT)) |
							((abs(theta)<std::numeric_limits<T>::epsilon()||abs(theta-M_PI)<std::numeric_limits<T>::epsilon())?ALIGN_VCENTER:((theta>M_PI)?ALIGN_TOP:ALIGN_BOTTOM)),
							FormatNumber(180 * theta / M_PI, SCALE_LINEAR)));
			}
		}

		cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 10, 0.0f, 0.0f, 0.0f, 0.0f, axisY.grid.color));
		cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 10, 0.0f, 0.0f, 0.0f, 0.0f, axisY.grid.style));
		cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 10, 0.0f, 0.0f, 0.0f, 0.0f, axisY.tick.color));
		for (T r : axisY.calculateTicks()) {
			if (r <= axisY.min)
				continue;
			std::pair<float, float> edge = toPolarPixel(0.0, r);
			float pixelR = std::abs(edge.first - cx);
			if (pixelR > 0) {
				if (axisY.grid.visible) {
					cmds.push_back(plt_render_cmd_(OP_DRAW_ELLIPSE, 10, cx, cy, pixelR, pixelR, ""));
				}
				if (axisY.tick.visible) {
					cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 10, edge.first, edge.second, 0.0f, ALIGN_HCENTER | ALIGN_TOP, FormatNumber(r, SCALE_LINEAR, axisY.unit)));
				}
			}
		}

		if (axisX.visible) {
			if (max_pixel_r > 0) {
				cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 50, 0.0f, 0.0f, axisX.width, 0.0f, ""));
				cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 50, 0.0f, 0.0f, 0.0f, 0.0f, axisX.color));
				cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 50, 0.0f, 0.0f, 0.0f, 0.0f, axisX.style));
				cmds.push_back(plt_render_cmd_(OP_DRAW_ELLIPSE, 50, cx, cy, max_pixel_r, max_pixel_r, ""));
			}
		}

		if (axisY.visible) {
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 50, 0.0f, 0.0f, axisY.width, 0.0f, ""));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 50, 0.0f, 0.0f, 0.0f, 0.0f, axisY.color));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 50, 0.0f, 0.0f, 0.0f, 0.0f, axisY.style));

			std::pair<float, float> max_edge2;

			//max_edge = toPolarPixel(0.0, axisY.max);
			max_edge2 = toPolarPixel(M_PI, axisY.max);
			cmds.push_back(plt_render_cmd_(OP_MOVETO, 50, max_edge.first, max_edge.second, 0.0f, 0.0f, ""));
			cmds.push_back(plt_render_cmd_(OP_LINETO, 50, max_edge2.first, max_edge2.second, 0.0f, 0.0f, ""));

			if (!axisY.label.empty()) {
				cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 50, 0.0f, 0.0f, 0.0f, 0.0f, axisY.color));
				cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 50, max_edge.first+fontWidth, max_edge.second, 0, ALIGN_LEFT | ALIGN_VCENTER, axisY.label));
			}

			max_edge = toPolarPixel(M_PI_2, axisY.max);
			max_edge2 = toPolarPixel(3 * M_PI_2, axisY.max);
			cmds.push_back(plt_render_cmd_(OP_MOVETO, 50, max_edge.first, max_edge.second, 0.0f, 0.0f, ""));
			cmds.push_back(plt_render_cmd_(OP_LINETO, 50, max_edge2.first, max_edge2.second, 0.0f, 0.0f, ""));
		}

		if (!title.empty()) {
			max_edge = toPolarPixel(M_PI_2, axisY.max);
			cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 50, 0.0f, 0.0f, 0.0f, 0.0f, axisX.color));
			float yPos = max_edge.second - 2 * fontHeight;
			if (yPos < 0) yPos = 0;
			cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 50, max_edge.first, yPos, 0, ALIGN_HCENTER | ALIGN_TOP, title));
		}

	} else if (coord == COORD_CARTESIAN) {

		float pxMin = toPixelX(axisX.min);
		float pxMax = toPixelX(axisX.max);
		float pyMin = toPixelY(axisY.max);//!
		float pyMax = toPixelY(axisY.min);//!
		rect = {{pxMin, pyMin}, {pxMax, pyMax}};

		cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 5, 0.0f, 0.0f, 0.0f, 0.0f, ""));
		cmds.push_back(plt_render_cmd_(OP_DRAW_RECT, 5, pxMin, pyMin, pxMax-pxMin, pyMax-pyMin, backgroundColor));

		int nCharsYTicks = 0, nCharsY2Ticks = 0;

		cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 10, 0.0f, 0.0f, 1.0f, 0.0f, ""));

		cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 10, 0.0f, 0.0f, 0.0f, 0.0f, axisX.grid.color));
		cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 10, 0.0f, 0.0f, 0.0f, 0.0f, axisX.grid.style));
		cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 10, 0.0f, 0.0f, 0.0f, 0.0f, axisX.tick.color));
		for (T x : axisX.calculateTicks()) {
			float px = toPixelX(x);

			if (axisX.grid.visible) {
				cmds.push_back(plt_render_cmd_(OP_MOVETO, 10, px, pyMin, 0.0f, 0.0f, ""));
				cmds.push_back(plt_render_cmd_(OP_LINETO, 10, px, pyMax, 0.0f, 0.0f, ""));
			}

			if (axisX.tick.visible) {
				cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 10, px, pyMax, 0.0f, ALIGN_HCENTER | ALIGN_TOP, FormatNumber(x, axisX.scale, axisX.unit)));
			}
		}

		cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 10, 0.0f, 0.0f, 0.0f, 0.0f, axisY.grid.color));
		cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 10, 0.0f, 0.0f, 0.0f, 0.0f, axisY.grid.style));
		cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 10, 0.0f, 0.0f, 0.0f, 0.0f, axisY.tick.color));
		for (T y : axisY.calculateTicks()) {
			float py = toPixelY(y);

			if (axisY.grid.visible || (axisY2.visible && axisY2.grid.visible)) {
				cmds.push_back(plt_render_cmd_(OP_MOVETO, 10, pxMin, py, 0.0f, 0.0f, ""));
				cmds.push_back(plt_render_cmd_(OP_LINETO, 10, pxMax, py, 0.0f, 0.0f, ""));
			}

			if (axisY.tick.visible) {
				std::string s = FormatNumber(y, axisY.scale, axisY.unit);
				if (s.size() > nCharsYTicks)
					nCharsYTicks = s.size();
				cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 10, pxMin, py, 0.0f, ALIGN_RIGHT | ALIGN_VCENTER, s));
			}
		}

		if (axisY2.visible) {
			for (T y2 : axisY2.calculateTicks()) {
				float py = toPixelY(y2, AXIS_SECONDARY);
				if (axisY.tick.visible) {
					cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 10, 0.0f, 0.0f, 0.0f, 0.0f, axisY2.tick.color));

					std::string s = FormatNumber(y2, axisY2.scale, axisY2.unit);
					if (s.size() > nCharsY2Ticks)
						nCharsY2Ticks = s.size();
					cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 10, pxMax, py, 0.0f, ALIGN_LEFT | ALIGN_VCENTER, s));
				}
			}
		}

		if (axisX.visible) {
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 50, 0.0f, 0.0f, 0.0f, 0.0f, axisX.color));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 50, 0.0f, 0.0f, 0.0f, 0.0f, axisX.style));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 50, 0.0f, 0.0f, axisX.width, 0.0f, ""));

			cmds.push_back(plt_render_cmd_(OP_MOVETO, 50, pxMin, pyMax, 0.0f, 0.0f, ""));
			cmds.push_back(plt_render_cmd_(OP_LINETO, 50, pxMax, pyMax, 0.0f, 0.0f, ""));

			if (!axisX.label.empty()) {
				cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 50, 0.0f, 0.0f, 0.0f, 0.0f, axisX.color));

				float yPos = pyMax + fontHeight;
				if (yPos > plotHeight) yPos = plotHeight;
				cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 50, (pxMin+pxMax)/2, yPos, 0.0f, ALIGN_HCENTER | ALIGN_TOP, axisX.label));
			}
		}
		if (axisY.visible) {
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 50, 0.0f, 0.0f, 0.0f, 0.0f, axisY.color));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 50, 0.0f, 0.0f, 0.0f, 0.0f, axisY.style));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 50, 0.0f, 0.0f, axisY.width, 0.0f, ""));

			cmds.push_back(plt_render_cmd_(OP_MOVETO, 50, pxMin, pyMin, 0.0f, 0.0f, ""));
			cmds.push_back(plt_render_cmd_(OP_LINETO, 50, pxMin, pyMax, 0.0f, 0.0f, ""));

			if (!axisY.label.empty()) {
				cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 50, 0.0f, 0.0f, 0.0f, 0.0f, axisY.color));
				float xPos = pxMin-(1+nCharsYTicks)*fontWidth;
				if (xPos < 0) xPos = 0;
				cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 50, xPos, (pyMin+pyMax)/2, M_PI_2, ALIGN_HCENTER | ALIGN_TOP, axisY.label));
			}
		}
		if (axisY2.visible) {
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 50, 0.0f, 0.0f, 0.0f, 0.0f, axisY2.color));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 50, 0.0f, 0.0f, 0.0f, 0.0f, axisY2.style));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 50, 0.0f, 0.0f, axisY2.width, 0.0f, ""));

			cmds.push_back(plt_render_cmd_(OP_MOVETO, 50, pxMax, pyMin, 0.0f, 0.0f, ""));
			cmds.push_back(plt_render_cmd_(OP_LINETO, 50, pxMax, pyMax, 0.0f, 0.0f, ""));

			if (!axisY2.label.empty()) {
				cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 50, 0.0f, 0.0f, 0.0f, 0.0f, axisY2.color));

				float xPos = pxMax+nCharsY2Ticks*fontWidth;
				if (xPos > plotWidth) xPos = plotWidth;
				cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 50, xPos, (pyMin+pyMax)/2, M_PI_2, ALIGN_HCENTER | ALIGN_BOTTOM, axisY2.label));
			}
		}

		if (!title.empty()) {
			cmds.push_back(plt_render_cmd_(OP_SET_TEXT_COLOR, 50, 0.0f, 0.0f, 0.0f, 0.0f, axisX.color));
			float yPos = pyMin - 2 * fontHeight;
			if (yPos < 0) yPos = 0;
			cmds.push_back(plt_render_cmd_(OP_DRAW_TEXT, 50, (pxMin+pxMax)/2, yPos, 0, ALIGN_HCENTER | ALIGN_TOP, title));
		}
	}

	for (const auto* curve : curves) {
		const std::vector<T> &xData = curve->getXData();
		const std::vector<T> &yData = curve->getYData();

		if (!curve || xData.empty() || yData.empty())
			continue;
		if (!curve->visible)
			continue;

		std::vector<std::pair<float, float>> pxpy;

		cmds.push_back(plt_render_cmd_(OP_SET_LINE_WIDTH, 20, 0.0f, 0.0f, curve->width, 0.0f, ""));
		cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 20, 0.0f, 0.0f, 0.0f, 0.0f, curve->color));
		cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 20, 0.0f, 0.0f, 0.0f, 0.0f, curve->style));

		const Axis<T>& axisY_ = (curve->axisY == AXIS_SECONDARY) ? axisY2 : axisY;

		float px;
		float py;

		bool prevflag = false;
		bool currflag = false;
		if (coord == COORD_CARTESIAN) {
			px = toPixelX(xData[0]);
			py = toPixelY(yData[0], curve->axisY);
			if (xData[0] >= axisX.min && xData[0] < axisX.max &&
				yData[0] >= axisY_.min && yData[0] < axisY_.max) {
				if (curve->markerShape != "")
					pxpy.push_back({px, py});
				prevflag = true;
			}
		} else if (coord == COORD_POLAR) {
			auto tmp = toPolarPixel(xData[0], yData[0]);
			px = tmp.first;
			py = tmp.second;
			if (yData[0] >= axisY.min && yData[0] < axisY.max) {
				if (curve->markerShape != "")
					pxpy.push_back(tmp);
				prevflag = true;
			}
		}

		std::pair<float, float> prevPt{px, py}, currPt;

		bool updated = false;
		for (size_t i = 1; i < std::min(xData.size(), yData.size()); ++i) {
			if (coord == COORD_CARTESIAN) {
				px = toPixelX(xData[i]);
				py = toPixelY(yData[i], curve->axisY);
			} else if (coord == COORD_POLAR) {
				auto p = toPolarPixel(xData[i], yData[i]);
				px = p.first;
				py = p.second;
			}
			currPt = {px, py};

			if (coord == COORD_CARTESIAN) {
				if (xData[i] >= axisX.min && xData[i] < axisX.max &&
					yData[i] >= axisY_.min && yData[i] < axisY_.max) {
					currflag = true;
					if (curve->markerShape != "")
						pxpy.push_back({px, py});
				}
			} else if (coord == COORD_POLAR) {
				if (yData[i] >= axisY.min && yData[i] < axisY.max) {
					currflag = true;
					if (curve->markerShape != "")
						pxpy.push_back({px, py});
				}
			}

			if (prevflag || currflag ||
				(coord == COORD_CARTESIAN && (checkSegmentIntersect<T>(prevPt, currPt,
																	   {rect.first.first, rect.first.second},
																	   {rect.second.first, rect.first.second}) ||
											  checkSegmentIntersect<T>(prevPt, currPt,
												                       {rect.second.first, rect.first.second},
																	   {rect.second.first, rect.second.second}) ||
											  checkSegmentIntersect<T>(prevPt, currPt,
												                       {rect.second.first, rect.second.second},
																	   {rect.first.first, rect.second.second}) ||
											  checkSegmentIntersect<T>(prevPt, currPt,
												                       {rect.first.first, rect.second.second},
																	   {rect.first.first, rect.first.second}))) ||
				(coord == COORD_POLAR && (true/*TODO*/))) {
				if (!updated)
					cmds.push_back(plt_render_cmd_(OP_MOVETO, 20, prevPt.first, prevPt.second, 0.0f, 0.0f, ""));
				cmds.push_back(plt_render_cmd_(OP_LINETO, 20, px, py, 0.0f, 0.0f, ""));
				updated = true;
			} else
				updated = false;

			prevflag = currflag; currflag = false;
			prevPt = currPt;
		}

		if (!pxpy.empty()) {
			float h = curve->markerSize / 2.0f;
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_COLOR, 30, 0.0f, 0.0f, 0.0f, 0.0f, curve->markerColor));
			cmds.push_back(plt_render_cmd_(OP_SET_LINE_STYLE, 30, 0.0f, 0.0f, 0.0f, 0.0f, "-"));
			for (auto [px, py] : pxpy) {
				if (curve->markerShape == "x") {
					cmds.push_back(plt_render_cmd_(OP_MOVETO, 30, px - h, py - h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, px + h, py + h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_MOVETO, 30, px - h, py + h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, px + h, py - h, 0.0f, 0.0f, ""));
				} else if (curve->markerShape == ".") {
					cmds.push_back(plt_render_cmd_(OP_DRAW_ELLIPSE, 30, px, py, h, h, curve->markerColor));
				} else if (curve->markerShape == "+") {
					cmds.push_back(plt_render_cmd_(OP_MOVETO, 30, px - h, py, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, px + h, py, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_MOVETO, 30, px, py - h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, px, py + h, 0.0f, 0.0f, ""));
				} else if (curve->markerShape == "o") {
					cmds.push_back(plt_render_cmd_(OP_DRAW_ELLIPSE, 30, px, py, h, h, ""));
				} else if (curve->markerShape == "s") {
					cmds.push_back(plt_render_cmd_(OP_DRAW_RECT, 30, px-h, py-h, 2 * h, 2 * h, ""));
					//cmds.push_back(plt_render_cmd_(OP_MOVETO, 30, px - h, py - h, 0.0f, 0.0f, ""));
					//cmds.push_back(plt_render_cmd_(OP_LINETO, 30, px + h, py - h, 0.0f, 0.0f, ""));
					//cmds.push_back(plt_render_cmd_(OP_LINETO, 30, px + h, py + h, 0.0f, 0.0f, ""));
					//cmds.push_back(plt_render_cmd_(OP_LINETO, 30, px - h, py + h, 0.0f, 0.0f, ""));
					//cmds.push_back(plt_render_cmd_(OP_LINETO, 30, px - h, py - h, 0.0f, 0.0f, ""));
				} else if (curve->markerShape == "^") {
					cmds.push_back(plt_render_cmd_(OP_MOVETO, 30, px, py - h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, px - h, py + h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, px + h, py + h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, px, py - h, 0.0f, 0.0f, ""));
				} else if (curve->markerShape == "d") {
					cmds.push_back(plt_render_cmd_(OP_MOVETO, 30, px, py - h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, px + h, py, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, px, py + h, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, px - h, py, 0.0f, 0.0f, ""));
					cmds.push_back(plt_render_cmd_(OP_LINETO, 30, px, py - h, 0.0f, 0.0f, ""));
				}
			}
		}
	}

	if (nout < cmds.size()) {
		printf("Warning: Insufficient plt_render_cmd slots for output: %d < %d\n", nout, cmds.size());
		fflush(stdout);
	}

	int nret = std::min(cmds.size(), nout);
	for (int i = 0; i < nret; i++) {
		out[i] = cmds.data()[i];
	}
	return nret;
}

template<typename T>
void Curve<T>::updateData(const std::vector<T> &x, const std::vector<T> &y)
{
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

template<typename T>
void Curve<T>::appendData(const std::vector<T> &x, const std::vector<T> &y)
{
	xData.insert(xData.end(), x.begin(), x.end());
	yData.insert(yData.end(), y.begin(), y.end());

	if (!x.empty()) {
		auto [min_it, max_it] = std::minmax_element(x.begin(), x.end());
		if (xmin > *min_it)
			xmin = *min_it;
		if (xmax < *max_it)
			xmax = *max_it;

		Axis<T> *axis = &plot->axisX;
		if (axis->autoscroll) {
			T delta = 0;

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

		Axis<T> *axis = nullptr;
		if (axisY == AXIS_PRIMARY)
			axis = &plot->axisY;
		else
			axis = &plot->axisY2;
		if (axis->autoscroll) {
			T delta = 0;

			if (ymax > axis->max)
				delta = (ymax - axis->max) + (axis->max - axis->min) / 2;
			else if (ymin < axis->min)
				delta = -((axis->min - ymin) + (axis->max - axis->min) / 2);

			axis->pan(delta);
		}
	}
}

template<typename T>
int Curve<T>::set_option(const std::string& option, const std::string& value)
{
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

template class Axis<float>;
template class Plot<float>;
template class Curve<float>;

template class Axis<double>;
template class Plot<double>;
template class Curve<double>;

