#ifndef PLOT_HPP
#define PLOT_HPP
#define _USE_MATH_DEFINES

#include <string>
#include <vector>
#include <map>

#include "libplot.h"

template <typename T>
class Plot;
template <typename T>
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

template <typename T>
struct Axis {
	static_assert(std::is_same_v<T, float> || std::is_same_v<T, double>, "Error: Must be float or double!");

	Plot<T> *plot = nullptr;

    int scale = SCALE_LINEAR;
    T min = 0.0f;
    T max = 100.0f;
	T ratioMargin = 0.08f;

	float width = 1.0f; // line width
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
	void applyMargin(int ends = END_MIN|END_MAX);
    void pan(T d);
	void zoom(T center, float factor);
	std::vector<T> calculateTicks() const;
};

template <typename T>
class Plot {
	static_assert(std::is_same_v<T, float> || std::is_same_v<T, double>, "Error: Must be float or double!");
private:
	friend class Axis<T>;
	friend class Curve<T>;

	int coord = COORD_CARTESIAN;

	float plotWidth = 400.0f; //pixels
	float plotHeight = 300.0f; //pixels
	float fontWidth = 12.0f; //pixels
	float fontHeight = 15.0f; //pixels

	std::map<std::string, float> paddings = {{"L", 60}, {"T", 40}, {"R", 60}, {"B", 40}}; //pixels

    std::string backgroundColor = ""; // empty for transparent
	std::string title = "";
    std::string titleColor = "#000000";

	std::vector<Curve<T> *> curves;
	Curve<T> *addCurve(Curve<T> *curve);
	Curve<T> *removeCurve(Curve<T> *curve);

    void applyMargin();
	void autoRange(bool force = true);
    float getClientWidth() const { return plotWidth - paddings.at("L") - paddings.at("R"); }
    float getClientHeight() const { return plotHeight - paddings.at("T") - paddings.at("B"); }
public:
	Axis<T> axisX;   // as theta in Polar
	Axis<T> axisY;   // primary (left) Y, as r in Polar
	Axis<T> axisY2;  // secondary (right) Y
	int activeY = AXIS_PRIMARY;

    Plot(int coord = COORD_CARTESIAN);
    ~Plot();

	int getCoord() const { return coord; }

    float toPixelX(T mathX) const;
    float toPixelY(T mathY, int which = AXIS_PRIMARY) const;
    T toMathX(float pixelX) const;
    T toMathY(float pixelY, int which = AXIS_PRIMARY) const;
    std::pair<float, float> toPolarPixel(T theta, T r) const;
    std::pair<T, T> toPolarMath(float pixelX, float pixelY) const;
	void pix2math(float pixelX, float pixelY, T *x, T *y, T *y2) const;
    void pan(float pixelXDelta, float pixelYDelta);
    void zoom(float pixelX, float pixelY, float factor);
    int set_option(const std::string& option, const std::string& value);
	int render(struct plt_render_cmd *out, size_t nout) const;
};

template <typename T>
struct Curve {
	static_assert(std::is_same_v<T, float> || std::is_same_v<T, double>, "Error: Must be float or double!");

	Plot<T> *plot = nullptr;
	int axisY = AXIS_PRIMARY;

	bool visible = true;
    float width = 2.0f; // pixels
    std::string label = "";
    std::string color = "#000000";
    std::string style = "-";

	std::string markerShape = "";
	float markerSize = 6.0f; // pixels
	std::string markerColor = "#000000";

    Curve(Plot<T> *plot) {
		this->plot = plot;
		axisY = plot->activeY;

		plot->addCurve(this);
	}
    ~Curve() {
		plot->removeCurve(this);
	}
	const std::vector<T> &getXData() const { return xData; }
	const std::vector<T> &getYData() const { return yData; }
	std::pair<T, T> getXMinMax() const { return {xmin, xmax}; }
	std::pair<T, T> getYMinMax() const { return {ymin, ymax}; }

	void updateData(const std::vector<T> &x, const std::vector<T> &y);
	void appendData(const std::vector<T> &x, const std::vector<T> &y);
    int set_option(const std::string& option, const std::string& value);

private:
    std::vector<T> xData;
    std::vector<T> yData;

	// cache
	T xmin, xmax, ymin, ymax;
};

#endif // PLOT_HPP
