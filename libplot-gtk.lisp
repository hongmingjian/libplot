(eval-when (:compile-toplevel :load-toplevel :execute)
  (ql:quickload '(:cffi :cl-cffi-gtk float-features) :silent t))

(in-package :cl-user)

(defpackage :libplot
  (:use :cl)
  (:export #:plt-create
           #:plt-destroy
           #:plt-set-option
           #:plt-pan
           #:plt-zoom
           #:plt-pix2math
           #:plt-curve
           #:plt-remove
           #:plt-append
           #:plt-render
           #:execute-render-cmds
           #:+coord-cartesian+
           #:+coord-polar+
           #:plt-render-cmd
           #:demo))

(in-package :libplot)

(defvar *dir-of-this-file*
  (let ((path (or *load-pathname* *compile-file-pathname*)))
    (if path
        (uiop:pathname-directory-pathname path)
        (uiop:getcwd))))

(cffi:define-foreign-library libplot
  (:windows "libplot.dll")
  (:unix (:or "libplot.so" "libplot.dylib"))
  (t (:default "libplot")))

(cffi:use-foreign-library libplot)

(cffi:defcstruct plt-render-cmd
  (cmd  :unsigned-int)
  (x    :float)
  (y    :float)
  (arg1 :float)
  (arg2 :float)
  (arg3 (:array :char 64)))

(declaim (inline cmd-op cmd-z-index))
(defun cmd-op (cmd)      (ldb (byte 8 0) cmd))
(defun cmd-z-index (cmd) (ldb (byte 8 8) cmd))

(defconstant +coord-cartesian+ 0)
(defconstant +coord-polar+ 1)

(defconstant +op-set-pen-color+ 1)
(defconstant +op-set-pen-style+ 2)
(defconstant +op-set-pen-width+ 3)
(defconstant +op-moveto+        4)
(defconstant +op-lineto+        5)
(defconstant +op-draw-text+     6)
(defconstant +op-draw-ellipse+  7)
(defconstant +op-draw-rect+     8)

(defconstant +align-left+     #x00000000)
(defconstant +align-hcenter+  #x00000001)
(defconstant +align-right+    #x00000002)
(defconstant +align-top+      #x00000000)
(defconstant +align-vcenter+  #x00010000)
(defconstant +align-bottom+   #x00020000)
(defconstant +align-baseline+ #x00040000)

(cffi:defcfun ("plt_create" plt-create) :pointer
  (coord :int))

(cffi:defcfun ("plt_destroy" plt-destroy) :void
  (plt :pointer))

(cffi:defcfun ("plt_set_option" plt-set-option) :int
  (handle :pointer)
  (option :string)
  (value  :string))

(cffi:defcfun ("plt_pan" plt-pan) :void
  (plt :pointer)
  (dx :float)
  (dy :float))

(cffi:defcfun ("plt_zoom" plt-zoom) :void
  (plt :pointer)
  (cx :float)
  (cy :float)
  (factor :float))

(cffi:defcfun ("plt_pix2math" plt-pix2math) :void
  (plt :pointer)
  (px :float)
  (py :float)
  (x :pointer)
  (y :pointer)
  (y2 :pointer))

(cffi:defcfun ("plt_curve" %plt-curve) :pointer
  (handle :pointer)
  (x :pointer)
  (y :pointer)
  (count :int))

(defun plt-curve (handle x y count)
  (if (zerop count)
      (%plt-curve handle (cffi:null-pointer) (cffi:null-pointer) count)
      (cffi:with-foreign-objects ((x-arr :float count)
                                  (y-arr :float count))
        (loop for i from 0 below count
              do (setf (cffi:mem-aref x-arr :float i) (aref x i)
                       (cffi:mem-aref y-arr :float i) (aref y i)))
        (%plt-curve handle x-arr y-arr count))))

(cffi:defcfun ("plt_append" %plt-append) :void
  (crv :pointer)
  (x :pointer)
  (y :pointer)
  (count :int))

(defun plt-append (crv x y count)
  (cffi:with-foreign-objects ((x-arr :float count)
                              (y-arr :float count))
    (loop for i from 0 below count
          do (setf (cffi:mem-aref x-arr :float i) (aref x i)
                   (cffi:mem-aref y-arr :float i) (aref y i)))
    (%plt-append crv x-arr y-arr count)))

(cffi:defcfun ("plt_remove" plt-remove) :void
  (crv :pointer))

(cffi:defcfun ("plt_render" plt-render) :int
  (plt :pointer)
  (rcmds :pointer)
  (count :int))

(defun parse-color (color)
  (let ((rgba (gdk:gdk-rgba-parse color)))
    (if rgba
        (list (gdk:gdk-rgba-red rgba)
              (gdk:gdk-rgba-green rgba)
              (gdk:gdk-rgba-blue rgba)
              (gdk:gdk-rgba-alpha rgba))
        '(0.0 0.0 0.0 0.0))))

(defun parse-style (style)
  (when (stringp style)
    (let* ((s (string-trim " \t\r\n" style)))
      (cond
        ((string= s "-") '(1)) ; solid
        ((string= s "--") '(6 4)) ; dash
        ((string= s ":") '(2 2)) ; dot
        ((string= s "-.") '(6 3 2 3)) ; dashdot
        ((string= s "-..") '(6 3 2 3 2 3)) ;dashdotdot
        (t nil) ; = PS_NULL in GDI
        ))))

(defun execute-render-cmd (cr rcmd draw-state)
  (let* ((cmd    (cffi:foreign-slot-value rcmd '(:struct plt-render-cmd) 'cmd))
         (op     (cmd-op cmd))
         (x      (cffi:foreign-slot-value rcmd '(:struct plt-render-cmd) 'x))
         (y      (cffi:foreign-slot-value rcmd '(:struct plt-render-cmd) 'y))
         (arg1   (cffi:foreign-slot-value rcmd '(:struct plt-render-cmd) 'arg1))
         (arg2   (cffi:foreign-slot-value rcmd '(:struct plt-render-cmd) 'arg2))
         (_arg3   (cffi:foreign-slot-pointer rcmd '(:struct plt-render-cmd) 'arg3))
         (arg3   (cffi:foreign-string-to-lisp _arg3 :max-chars 64))
         )
;    (format t "~A ~A ~A ~A ~A \"~A\"~%" op x y arg1 arg2 arg3)
    (case op
      (#.+op-set-pen-color+
       (apply #'cairo:cairo-set-source-rgba cr (parse-color arg3)))

      (#.+op-set-pen-style+
       (let ((style (parse-style arg3)))
         (setf (getf draw-state :pen-style) style)))

      (#.+op-set-pen-width+
       (cairo:cairo-set-line-width cr arg1))

      (#.+op-moveto+
       (setf (getf draw-state :x) x
             (getf draw-state :y) y))

      (#.+op-lineto+
       (let ((cur-x (getf draw-state :x))
             (cur-y (getf draw-state :y))
             (style (getf draw-state :pen-style))
             )
         (when style
           (cairo:cairo-move-to cr cur-x cur-y)
           (cairo:cairo-line-to cr x y)
           (cairo:cairo-set-dash cr (if (/= 1 (length style)) style nil) 0)
           (cairo:cairo-stroke cr)
           )
         (setf (getf draw-state :x) x
               (getf draw-state :y) y)))

      (#.+op-draw-text+
       (let* ((angle      arg1)
              (alignment  (truncate arg2))
              (text       arg3)
              (extents    (cairo:cairo-text-extents cr text))
              (width      (cairo:cairo-text-extents-t-width extents))
              (x-bearing  (cairo:cairo-text-extents-t-x-bearing extents))
              (y-bearing  (cairo:cairo-text-extents-t-y-bearing extents))
              (height     (cairo:cairo-text-extents-t-height extents))

              (ox (cond
                    ((plusp (logand alignment +align-hcenter+)) (/ width 2.0))
                    ((plusp (logand alignment +align-right+))  width)
                    (t                                         0.0)))
              (oy (cond
                    ((plusp (logand alignment +align-vcenter+))  (+ y-bearing (/ height 2.0)))
                    ((plusp (logand alignment +align-bottom+))   (+ y-bearing height))
                    ((plusp (logand alignment +align-baseline+)) 0)
                    (t                                           y-bearing))))

         (cairo:cairo-save cr)
         (cairo:cairo-translate cr x y)
         (cairo:cairo-rotate cr (- angle))
         (cairo:cairo-move-to cr (- (- ox) x-bearing) (- oy))
         (cairo:cairo-show-text cr text)
         (cairo:cairo-restore cr)))

      (#.+op-draw-ellipse+
       (let ((style (getf draw-state :pen-style))
             (fill-color (parse-color arg3))
             (rx arg1)
             (ry arg2))

         (cairo:cairo-save cr)
         (cairo:cairo-translate cr x y)
         (cairo:cairo-new-sub-path cr)
         (cairo:cairo-scale cr rx ry)
         (cairo:cairo-arc cr 0.0 0.0 1.0 0.0 (* 2.0 pi))
         (apply #'cairo:cairo-set-source-rgba cr fill-color)
         (cairo:cairo-fill-preserve cr)
         (cairo:cairo-restore cr)

         (when style
           (cairo:cairo-set-dash cr (if (/= 1 (length style)) style nil) 0)
           (cairo:cairo-stroke cr))
         ))

      (#.+op-draw-rect+
       (let ((style (getf draw-state :pen-style))
             (fill-color (parse-color arg3))
             )

         (cairo:cairo-save cr)
         (cairo:cairo-rectangle cr x y arg1 arg2)
         (apply #'cairo:cairo-set-source-rgba cr fill-color)
         (cairo:cairo-fill-preserve cr)
         (cairo:cairo-restore cr)

         (when style
           (cairo:cairo-set-dash cr (if (/= 1 (length style)) style nil) 0)
           (cairo:cairo-stroke cr))
         ))

      (t (error "unknown op=~A~%" op)))))

(defun execute-render-cmds (cr ptr-rcmds count)
  (declare (type fixnum count))
  (when (> count 0)
    (let* ((struct-size (cffi:foreign-type-size '(:struct plt-render-cmd)))
           (indices (loop for i of-type fixnum from 0 below count collect i)))
      (setf indices
            (stable-sort indices #'<
                         :key (lambda (i)
                                (let* ((ptr-rcmd (cffi:inc-pointer ptr-rcmds (* i struct-size)))
                                       (cmd (cffi:foreign-slot-value ptr-rcmd '(:struct plt-render-cmd) 'cmd)))
                                  (cmd-z-index cmd)))))
      (let ((draw-state (list :x 0.0 :y 0.0
                              :pen-style nil
                              ))
            )
        (cairo:cairo-save cr)

        (dolist (i indices)
          (let ((ptr-rcmd (cffi:inc-pointer ptr-rcmds (* i struct-size))))
            (execute-render-cmd cr ptr-rcmd draw-state)))

        (cairo:cairo-restore cr)
        ))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
(defun archimedean-spiral (nturns &key (points-per-turn 72) (polar nil))
  (declare (optimize (speed 3) (safety 0) (debug 0))
           (type fixnum nturns points-per-turn))
  (let* ((npoints (1+ (* nturns points-per-turn)))
         (x-vec (make-array npoints :element-type 'single-float))
         (y-vec (make-array npoints :element-type 'single-float))
         (a 0.0d0)
         (b 1.0d0))
    (loop for i of-type fixnum from 0 below npoints
          for theta = (* i (/ (* 2.0d0 pi) points-per-turn))
          for r = (+ a (* b theta))
          do (setf (aref x-vec i) (float (if polar (rem theta (* 2.0d0 pi)) (* r (cos theta))) 1.0f0)
                   (aref y-vec i) (float (if polar r (* r (sin theta))) 1.0f0)))
    (values x-vec y-vec npoints)))

(defun rose-curve (k &key (points-per-turn 72) (polar nil))
  (declare (optimize (speed 3) (safety 0) (debug 0))
           (type fixnum k points-per-turn))
  (let* ((b 1.0d0)
         (max-angle (* 2.0d0 pi))
         (npoints (1+ (the fixnum (round (* k points-per-turn)))))
         (x-vec (make-array npoints :element-type 'single-float))
         (y-vec (make-array npoints :element-type 'single-float))
         (astep (/ max-angle (1- npoints))))
    (loop for i of-type fixnum from 0 below npoints
          for theta = (* i astep)
          for r = (* b (cos (* k theta)))
          do (setf (aref x-vec i) (float (if polar theta (* r (cos theta))) 1.0f0)
                   (aref y-vec i) (float (if polar r (* r (sin theta))) 1.0f0)))
    (values x-vec y-vec npoints)))

(defstruct app-state
  (coord +coord-cartesian+)
  (plot nil)
  (curves nil)
  (timer nil)
  (last-x 0.0)
  (last-y 0.0)
  window
  )

(defun demo (&optional (coord +coord-cartesian+))
  (float-features:with-float-traps-masked (:invalid :divide-by-zero :overflow :underflow :inexact)
    (gtk:within-main-loop
      (let* ((window (make-instance 'gtk:gtk-window
                                    :type :toplevel
                                    :title "Libplot demo"
                                    :default-width 800
                                    :default-height 600
                                    ))
             (drawing-area (make-instance 'gtk:gtk-drawing-area))
             (state (make-app-state :coord coord
                                    :window window
                                    )))

        (gtk:gtk-widget-add-events drawing-area '(:pointer-motion-mask
                                                   :button-press-mask
                                                   :button-release-mask
                                                   :button1-motion-mask
                                                   :scroll-mask
                                                   :key-press-mask))

        (g:g-signal-connect window "destroy"
                            (lambda (widget)
                              (declare (ignore widget))
                              (let ((plot (app-state-plot state)))
                                (unless (or (null plot) (cffi:null-pointer-p plot))
                                  (plt-destroy plot)))
                              (gtk:leave-gtk-main)))

        (g:g-signal-connect drawing-area "draw"
                            (lambda (widget cr)
                              (declare (ignore widget))
                              (let ((plot (app-state-plot state))
                                    (ptr (gobject:pointer cr))
                                    )
                                (unless (or (null plot) (cffi:null-pointer-p plot))
                                  (let ((max-rcmds 1024))
                                    (cffi:with-foreign-object (ptr-rcmds '(:struct plt-render-cmd) max-rcmds)
                                      (let ((num-rcmds (plt-render plot ptr-rcmds max-rcmds)))
                                        (execute-render-cmds ptr ptr-rcmds num-rcmds))))))
                              gdk:+gdk-event-stop+))

        (g:g-signal-connect drawing-area "configure-event"
                            (lambda (widget event)
                              (declare (ignore widget))
                              (let* ((width (gdk:gdk-event-configure-width event))
                                     (height (gdk:gdk-event-configure-height event))
                                     (plot (app-state-plot state)))
                                (unless (or (null plot) (cffi:null-pointer-p plot))
                                  (let ((size-str (format nil "~dx~d" width height)))
                                    (plt-set-option plot "plot.size" size-str)
                                    (gtk:gtk-widget-queue-draw drawing-area))))
                              t))

        (g:g-signal-connect drawing-area "scroll-event"
                            (lambda (widget event)
                              (let ((plot (app-state-plot state))
                                    (scroll-state (gdk:gdk-event-scroll-state event))
                                    )
                                (when (and scroll-state (member :control-mask scroll-state))
                                  (let* ((direction (gdk:gdk-event-scroll-direction event))
                                         (x (gdk:gdk-event-scroll-x event))
                                         (y (gdk:gdk-event-scroll-y event))
                                         (factor (cond
                                                   ((eq direction :up) 1.25)
                                                   ((eq direction :down) (/ 1.25))
                                                   (t 1.0)))
                                         )
                                    (when (/= factor 1.0)
                                      (plt-zoom plot (float x 1.0f0) (float y 1.0f0) (float factor 1.0f0))
                                      (gtk:gtk-widget-queue-draw widget))
                                    t)))))

        (g:g-signal-connect drawing-area "motion-notify-event"
                            (lambda (widget event)
                              (declare (ignore widget))
                              (let* ((x (gdk:gdk-event-motion-x event))
                                     (y (gdk:gdk-event-motion-y event))
                                     (plot (app-state-plot state)))
                                (cffi:with-foreign-objects ((x-ref :float)
                                                            (y-ref :float)
                                                            (y2-ref :float))
                                  (plt-pix2math plot (float x 1.0f0) (float y 1.0f0) x-ref y-ref y2-ref)
                                  (let ((lx (cffi:mem-ref x-ref :float))
                                        (ly (cffi:mem-ref y-ref :float))
                                        (ly2 (cffi:mem-ref y2-ref :float)))
                                    (setf (gtk:gtk-window-title window)
                                          (format nil "Libplot demo: (~A, ~A) -> (~A, ~A, ~A)" x y lx ly ly2))))

                                (when (member :button1-mask (gdk:gdk-event-motion-state event))
                                  (let* ((dx (- x (app-state-last-x state)))
                                         (dy (- y (app-state-last-y state))))
                                    (plt-pan plot (float dx 1.0f0) (float dy 1.0f0))
                                    (setf (app-state-last-x state) x
                                          (app-state-last-y state) y)
                                    (gtk:gtk-widget-queue-draw drawing-area))))
                              t))

        (g:g-signal-connect drawing-area "button-press-event"
                            (lambda (widget event)
                              (declare (ignore widget))
                              (when (= (gdk:gdk-event-button-button event) 1)
                                (setf (app-state-last-x state) (gdk:gdk-event-button-x event)
                                      (app-state-last-y state) (gdk:gdk-event-button-y event)))
                              t))

        (let ((plot (plt-create coord)))
          (if (cffi:null-pointer-p plot)
              (error "Failed to create a plot")
              (progn
                (setf (app-state-plot state) plot)
                (plt-set-option plot "plot.title" "Rose curve")
                (plt-set-option plot "plot.title.color" "red")
                (plt-set-option plot "plot.background.color" "#aaaaaa")
                (plt-set-option plot "plot.axisX.label" "Frequency")
                (plt-set-option plot "plot.axisX.unit" "Hz")
                (plt-set-option plot "plot.axisY.label" "a.u.")

                (let ((timer-id (glib:g-timeout-add 1000
                                  (lambda ()
                                    (let ((crvs (app-state-curves state)))
                                      (multiple-value-bind (x-vec y-vec count)
                                        (rose-curve (+ 3 (random 3))
                                                    :polar (/= coord +coord-cartesian+))
                                        (if (> (length crvs) 0)
                                            (plt-curve (first crvs) x-vec y-vec count)
                                            (let ((crv (plt-curve plot x-vec y-vec count)))
                                              (unless (cffi:null-pointer-p crv)
                                                (plt-set-option crv "curve.color" "#0000FF")
                                                (plt-set-option crv "curve.width" "2")
                                                (plt-set-option crv "curve.marker.color" "#FF0000")
                                                (plt-set-option crv "curve.marker.shape" "s")
                                                (push crv crvs)
                                                (setf (app-state-curves state) crvs))))
                                        (gtk:gtk-widget-queue-draw drawing-area)))
                                    nil ; don't repeat
                                    ))))
                  (setf (app-state-timer state) timer-id)))))

        (gtk:gtk-container-add window drawing-area)
        (gtk:gtk-widget-show-all window)))))
