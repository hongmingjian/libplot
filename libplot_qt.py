# -*- coding: utf-8 -*-

import ctypes

class LibPlot:
    class OpCode:
        OP_SET_PEN_COLOR = 1
        OP_SET_PEN_STYLE = 2
        OP_SET_PEN_WIDTH = 3
        OP_MOVETO = 4
        OP_LINETO = 5
        OP_DRAW_TEXT = 6
        OP_DRAW_ELLIPSE = 7
        OP_DRAW_RECT = 8

    class CoordSystem:
        COORD_CARTESIAN = 0
        COORD_POLAR = 1

    class Axis:
        AXIS_PRIMARY = 0
        AXIS_SECONDARY = 1

    class Scale:
        SCALE_LINEAR = 0
        SCALE_LOG = 1

    class Alignment:
        ALIGN_LEFT = 0x00000000
        ALIGN_HCENTER = 0x00000001
        ALIGN_RIGHT = 0x00000002
        ALIGN_TOP = 0x00000000
        ALIGN_VCENTER = 0x00010000
        ALIGN_BOTTOM = 0x00020000
        ALIGN_BASELINE = 0x00040000

    class PltRenderCmd(ctypes.Structure):
        _fields_ = [
            ("op", ctypes.c_uint, 8),
            ("zIndex", ctypes.c_uint, 8),
            ("unused", ctypes.c_uint, 16),
            ("x", ctypes.c_float),
            ("y", ctypes.c_float),
            ("arg1", ctypes.c_float),
            ("arg2", ctypes.c_float),
            ("arg3", ctypes.c_char * 64),
        ]
    def __init__(self, lib_path, use_double=False):
        self._lib = ctypes.CDLL(lib_path)

        self.real_t = ctypes.c_double if use_double else ctypes.c_float

        self._setup_signatures()

    def _setup_signatures(self):
        lib = self._lib
        real = self.real_t

        # void *plt_create(int coord);
        lib.plt_create.argtypes = [ctypes.c_int]
        lib.plt_create.restype = ctypes.c_void_p

        # void plt_destroy(void *plt);
        lib.plt_destroy.argtypes = [ctypes.c_void_p]
        lib.plt_destroy.restype = None

        # int plt_set_option(void *handle, const char *option, const char *value);
        lib.plt_set_option.argtypes = [
            ctypes.c_void_p,
            ctypes.c_char_p,
            ctypes.c_char_p,
        ]
        lib.plt_set_option.restype = ctypes.c_int

        # void plt_pan(void *plt, real dx, real dy);
        lib.plt_pan.argtypes = [ctypes.c_void_p, real, real]
        lib.plt_pan.restype = None

        # void plt_zoom(void *plt, real cx, real cy, float factor);
        lib.plt_zoom.argtypes = [ctypes.c_void_p, real, real, ctypes.c_float]
        lib.plt_zoom.restype = None

        # void plt_pix2math(void *plt, float px, float py, real *x, real *y, real *y2);
        lib.plt_pix2math.argtypes = [
            ctypes.c_void_p,
            ctypes.c_float,
            ctypes.c_float,
            ctypes.POINTER(real),
            ctypes.POINTER(real),
            ctypes.POINTER(real),
        ]
        lib.plt_pix2math.restype = None

        # void *plt_curve(void *handle, const real *x, const real *y, int count);
        lib.plt_curve.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(real),
            ctypes.POINTER(real),
            ctypes.c_int,
        ]
        lib.plt_curve.restype = ctypes.c_void_p

        # void plt_append(void *crv, const real *x, const real *y, int count);
        lib.plt_append.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(real),
            ctypes.POINTER(real),
            ctypes.c_int,
        ]
        lib.plt_append.restype = None

        # void plt_remove(void *crv);
        lib.plt_remove.argtypes = [ctypes.c_void_p]
        lib.plt_remove.restype = None

        # int plt_render(void *plt, struct plt_render_cmd *out, int nout);
        lib.plt_render.argtypes = [
            ctypes.c_void_p,
            ctypes.POINTER(LibPlot.PltRenderCmd),
            ctypes.c_int,
        ]
        lib.plt_render.restype = ctypes.c_int

    # --- Pythonic Methods ---
    def create(self, coord=CoordSystem.COORD_CARTESIAN):
        return self._lib.plt_create(coord)

    def destroy(self, plt_handle):
        self._lib.plt_destroy(plt_handle)

    def set_option(self, plt_handle, option, value):
        return self._lib.plt_set_option(
            plt_handle, option.encode("utf-8"), value.encode("utf-8")
        )

    def pan(self, plt_handle, dx, dy):
        self._lib.plt_pan(plt_handle, self.real_t(dx), self.real_t(dy))

    def zoom(self, plt_handle, cx, cy, factor):
        self._lib.plt_zoom(
            plt_handle,
            self.real_t(cx),
            self.real_t(cy),
            ctypes.c_float(factor),
        )

    def pix2math(self, plt_handle, px, py):
        x = self.real_t()
        y = self.real_t()
        y2 = self.real_t()
        self._lib.plt_pix2math(
            plt_handle,
            ctypes.c_float(px),
            ctypes.c_float(py),
            ctypes.byref(x),
            ctypes.byref(y),
            ctypes.byref(y2),
        )
        return x.value, y.value, y2.value

    def curve(self, plt_handle, x_list, y_list):
        count = len(x_list)
        if count != len(y_list):
            raise ValueError("x_list and y_list must have the same length")

        arr_type = self.real_t * count
        x_arr = arr_type(*x_list)
        y_arr = arr_type(*y_list)

        return self._lib.plt_curve(plt_handle, x_arr, y_arr, count)

    def append(self, crv_handle, x_list, y_list):
        count = len(x_list)
        if count != len(y_list):
            raise ValueError("x_list and y_list must have the same length")

        arr_type = self.real_t * count
        x_arr = arr_type(*x_list)
        y_arr = arr_type(*y_list)

        self._lib.plt_append(crv_handle, x_arr, y_arr, count)

    def remove(self, crv_handle):
        self._lib.plt_remove(crv_handle)

    def render(self, plt_handle, max_commands=2048):
        buf_type = LibPlot.PltRenderCmd * max_commands
        buf = buf_type()

        count = self._lib.plt_render(plt_handle, buf, max_commands)

        commands = []
        for i in range(count):
            cmd = buf[i]
            commands.append(
                {
                    "op": cmd.op,
                    "zIndex": cmd.zIndex,
                    "x": cmd.x,
                    "y": cmd.y,
                    "arg1": cmd.arg1,
                    "arg2": cmd.arg2,
                    "arg3": cmd.arg3.decode("utf-8", errors="ignore").rstrip(
                        "\x00"
                    ),
                }
            )
        return commands

import math
from PyQt5 import QtCore, QtGui
from PyQt5.QtCore import Qt

def execute_render_cmds(painter, commands):
    def convert_color(color_str):
        if not color_str:
            return QtGui.QColor(0, 0, 0)

        s = str(color_str).strip().lower()

        named_colors = {
            "red": QtGui.QColor(255, 0, 0),
            "green": QtGui.QColor(0, 128, 0),
            "blue": QtGui.QColor(0, 0, 255),
            "black": QtGui.QColor(0, 0, 0),
            "white": QtGui.QColor(255, 255, 255),
            "gray": QtGui.QColor(128, 128, 128),
            "yellow": QtGui.QColor(255, 255, 0)
        }

        if s in named_colors:
            return named_colors[s]

        if s.startswith("#"):
            s = s[1:]

        if len(s) == 6:
            try:
                color_val = int(s, 16)
                r = (color_val >> 16) & 0xFF
                g = (color_val >> 8) & 0xFF
                b = color_val & 0xFF
                return QtGui.QColor(r, g, b)
            except ValueError:
                pass

        return QtGui.QColor(0, 0, 0)


    def convert_pen_style(style_str):
        if style_str is None:
            return Qt.NoPen

        s = str(style_str)

        if s == "":
            return Qt.NoPen       # PS_NULL
        elif s == "-":
            return Qt.SolidLine   # PS_SOLID
        elif s == "--":
            return Qt.DashLine    # PS_DASH
        elif s == ":":
            return Qt.DotLine     # PS_DOT
        elif s == "-.":
            return Qt.DashDotLine # PS_DASHDOT
        elif s == "-..":
            return Qt.DashDotDotLine # PS_DASHDOTDOT

        return Qt.NoPen

    sorted_commands = sorted(commands, key=lambda cmd: cmd.get("zIndex", 0))

    painter.save()
    painter.setRenderHint(QtGui.QPainter.Antialiasing, True)

    pen_color = QtGui.QColor(0, 0, 0)
    pen_width = 1.0
    pen_style = Qt.NoPen

    last_x, last_y = 0.0, 0.0

    for cmd in sorted_commands:
        op = cmd.get("op", 0)
        x = cmd.get("x", 0.0)
        y = cmd.get("y", 0.0)
        arg1 = cmd.get("arg1", 0.0)
        arg2 = cmd.get("arg2", 0.0)
        arg3 = cmd.get("arg3", "")

        if op == 0:  # OP_NONE
            continue

        elif op == 1:  # OP_SET_PEN_COLOR (arg3: color)
            pen_color = convert_color(arg3)

        elif op == 2:  # OP_SET_PEN_STYLE (arg3: style)
            pen_style = convert_pen_style(arg3)

        elif op == 3:  # OP_SET_PEN_WIDTH (arg1: width)
            pen_width = max(0.5, float(arg1))

        elif op == 4:  # OP_MOVETO
            last_x, last_y = x, y

        elif op == 5:  # OP_LINETO
            painter.save()
            painter.setPen(QtGui.QPen(pen_color, pen_width, pen_style))
            painter.drawLine(int(last_x), int(last_y), int(x), int(y))
            painter.restore()
            last_x, last_y = x, y

        elif op == 6:  # OP_DRAW_TEXT
            painter.save()

            painter.translate(x, y)
            painter.rotate(-180 * arg1 / math.pi)
            painter.setPen(pen_color)

            metrics = painter.fontMetrics()
            if hasattr(metrics, "horizontalAdvance"):
                width = metrics.horizontalAdvance(arg3)
            else:
                width = metrics.width(arg3)
            height = metrics.height()

            align_val = int(arg2)
            h_align = align_val & 0x0000FFFF
            v_align = align_val & 0xFFFF0000

            dx, dy = 0, 0
            if h_align == 1:
                dx = -width / 2.0
            elif h_align == 2:
                dx = -width

            if v_align == 0x00010000:
                dy = height / 4.0
            elif v_align == 0x00020000:
                dy = 0
            else:
                dy = metrics.ascent()

            painter.drawText(int(dx), int(dy), arg3)
            painter.restore()

        elif op == 7:  # OP_DRAW_ELLIPSE (arg3: fill color)
            rx, ry = float(arg1), float(arg2)
            rect = QtCore.QRectF(x - rx, y - ry, rx * 2, ry * 2)
            painter.save()
            painter.setPen(QtGui.QPen(pen_color, pen_width, pen_style))
            fill_color = convert_color(arg3) if arg3 else None
            painter.setBrush(QtGui.QBrush(fill_color) if arg3 else Qt.NoBrush)
            painter.drawEllipse(rect)
            painter.restore()

        elif op == 8:  # OP_DRAW_RECT (arg3: fill color)
            w, h = float(arg1), float(arg2)
            rect = QtCore.QRectF(x, y, w, h)
            painter.save()
            painter.setPen(QtGui.QPen(pen_color, pen_width, pen_style))
            fill_color = convert_color(arg3) if arg3 else None
            painter.setBrush(QtGui.QBrush(fill_color) if arg3 else Qt.NoBrush)
            painter.drawRect(rect)
            painter.restore()

    painter.restore()


import os, sys
from PyQt5 import QtWidgets

def generate_rose_curve(k=4, a=1.0, num_points=360, polar=False):
    max_theta = 2 * math.pi
    theta = [i * max_theta / (num_points - 1) for i in range(num_points)]
    r = [a * math.cos(k * t) for t in theta]

    if polar:
        return theta, r
    else:
        x_vals = [r_val * math.cos(t) for r_val, t in zip(r, theta)]
        y_vals = [r_val * math.sin(t) for r_val, t in zip(r, theta)]
        return x_vals, y_vals

class MyWidget(QtWidgets.QWidget):

    def __init__(self, parent=None):
        super(MyWidget, self).__init__(parent)

        self.setWindowTitle("Libplot Demo")
        self.resize(600, 400)

        self.drag_start_pos = None
        self.setMouseTracking(True)
        try:
            cwd = os.path.dirname(os.path.abspath(__file__))
            lib_file = "libplot"
            if sys.platform.startswith("win"):
                lib_file += ".dll"
            elif sys.platform == "darwin":
                lib_file += ".dylib"
            else:
                lib_file += ".so"
            self.libplot = LibPlot(os.path.join(cwd, lib_file))
        except Exception as e:
            self.libplot = None
            print("Failed to load LibPlot: %s" % e)

        if self.libplot:
            coord = LibPlot.CoordSystem.COORD_POLAR#CARTESIAN
            self.plot = self.libplot.create(coord)
            self.libplot.set_option(self.plot, "plot.background.color", "#F0F0F0")

            x_vals, y_vals = generate_rose_curve(k=4.0, num_points=180,
                                                 polar=(coord != LibPlot.CoordSystem.COORD_CARTESIAN))
            self.curve_ptr = self.libplot.curve(self.plot, x_vals, y_vals)
            self.libplot.set_option(self.curve_ptr, "curve.color", "#0000FF")
            self.libplot.set_option(self.curve_ptr, "curve.marker.shape", "s")
            self.libplot.set_option(self.curve_ptr, "curve.marker.color", "#FF0000")

        self.installEventFilter(self)

    def closeEvent(self, event):
        if self.libplot:
            self.libplot.destroy(self.plot)
        QtWidgets.QWidget.closeEvent(self, event)

    def eventFilter(self, obj, event):
        if self.libplot:
            if obj == self:
                if event.type() == QtCore.QEvent.Resize:
                    w = obj.width()
                    h = obj.height()
                    self.libplot.set_option(
                        self.plot, "plot.size", "%dx%d" % (w, h)
                    )
                    obj.update()

                elif event.type() == QtCore.QEvent.Paint:
                    painter = QtGui.QPainter(obj)
                    cmds = self.libplot.render(self.plot, max_commands=2048)
                    execute_render_cmds(painter, cmds)
                    return True

                elif event.type() == QtCore.QEvent.MouseButtonPress:
                    if event.button() == QtCore.Qt.LeftButton:
                        self.drag_start_pos = event.pos()
                        return True

                elif event.type() == QtCore.QEvent.MouseMove:
                    current_pos = event.pos()
                    x, y, y2 = self.libplot.pix2math(
                        self.plot, current_pos.x(), current_pos.y()
                    )
                    self.setWindowTitle(
                        "Libplot demo: (%d, %d) -> (%f, %f, %f)"
                        % (current_pos.x(), current_pos.y(), x, y, y2)
                    )
                    if (
                        event.buttons() & QtCore.Qt.LeftButton
                        and self.drag_start_pos is not None
                    ):
                        dx = current_pos.x() - self.drag_start_pos.x()
                        dy = current_pos.y() - self.drag_start_pos.y()

                        self.libplot.pan(self.plot, float(dx), float(dy))
                        self.drag_start_pos = current_pos

                        obj.update()
                        return True

                elif event.type() == QtCore.QEvent.MouseButtonRelease:
                    if event.button() == QtCore.Qt.LeftButton:
                        self.drag_start_pos = None
                        return True

                elif event.type() == QtCore.QEvent.Wheel:
                    if event.modifiers() == QtCore.Qt.ControlModifier:
                        pos = event.pos()
                        cx, cy = float(pos.x()), float(pos.y())

                        angle = event.angleDelta().y()
                        factor = 1.25 if angle > 0 else 0.8

                        self.libplot.zoom(self.plot, cx, cy, factor)

                        obj.update()
                        return True

        return super().eventFilter(obj, event)

if __name__ == "__main__":
    app = QtWidgets.QApplication(sys.argv)
    window = MyWidget()
    window.show()
    sys.exit(app.exec_())
