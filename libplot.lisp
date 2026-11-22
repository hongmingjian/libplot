(eval-when (:compile-toplevel :load-toplevel :execute)
  (ql:quickload '(:cffi) :silent t))

(in-package :cl-user)

(defpackage :libplot
  (:use :cl)
  (:export #:+coord-cartesian+
           #:+coord-polar+
           #:plt-create
           #:plt-destroy
           #:plt-set-option
           #:plt-pan
           #:plt-zoom
           #:plt-pix2math
           #:plt-curve
           #:plt-remove
           #:plt-append
           #:plt-render
           #:demo))

(in-package :libplot)

(cffi:define-foreign-library libplot
  (:windows "libplot.dll")
  (:unix (:or "libplot.so" "libplot.dylib"))
  (t (:default "libplot")))

(cffi:defcstruct plt-render-cmd
  (cmd  :unsigned-int)
  (x    :float)
  (y    :float)
  (arg1 :float)
  (arg2 :float)
  (arg3 (:array :char 64)))

(declaim (inline cmd-op cmd-z-index cmd-arg3))
(defun cmd-op (rcmd)      (ldb (byte 8 0) rcmd))
(defun cmd-z-index (rcmd) (ldb (byte 8 8) rcmd))
(defun cmd-arg3 (rcmd)
  (cffi:foreign-string-to-lisp (cffi:foreign-slot-pointer rcmd '(:struct plt-render-cmd) 'arg3)
                               :max-chars 64))

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

(cffi:defcfun ("plt_render" %plt-render) :int
  (plt :pointer)
  (rcmds :pointer)
  (count :int))

(defun parse-style (style)
  (when (stringp style)
    (let ((s (string-trim " \t" style)))
      (unless (string= s "")
        (cond
          ((string= s "-")   '(1))           ; solid
          ((string= s "--")  '(6 4))         ; dash
          ((string= s ":")   '(2 2))         ; dot
          ((string= s "-.")  '(6 3 2 3))     ; dashdot
          ((string= s "-..") '(6 3 2 3 2 3)) ; dashdotdot
          (t nil))))))                       ; PS_NULL

(defun archimedean-spiral (nturns &key (points-per-turn 72) (polar nil))
  (declare (optimize (speed 3) (safety 0) (debug 0))
           (type fixnum nturns points-per-turn))
  (let* ((a 0.0d0)
         (b 1.0d0)
         (npoints (1+ (* nturns points-per-turn)))
         (astep (/ (* 2.0d0 pi) points-per-turn))
         (x-vec (make-array npoints :element-type 'single-float #+lispworks :allocation #+lispworks :static))
         (y-vec (make-array npoints :element-type 'single-float #+lispworks :allocation #+lispworks :static))
         )
    (loop for i of-type fixnum from 0 below npoints
          for theta = (* i astep)
          for r = (+ a (* b theta))
          do (setf (aref x-vec i) (float (if polar
                                             (rem theta (* 2.0d0 pi))
                                             (* r (cos theta)))
                                          1.0f0)
                   (aref y-vec i) (float (if polar
                                             r
                                             (* r (sin theta)))
                                          1.0f0)))
    (values x-vec y-vec npoints)))

(defun rose-curve (npetals &key (points-per-petal 72) (polar nil))
  (declare (optimize (speed 3) (safety 0) (debug 0))
           (type fixnum npetals points-per-petal))
  (let* ((b 1.0d0)
         (npoints (round (* npetals points-per-petal)))
         (astep (/ (* 2.0d0 pi) (1- npoints)))
         (x-vec (make-array npoints :element-type 'single-float #+lispworks :allocation #+lispworks :static))
         (y-vec (make-array npoints :element-type 'single-float #+lispworks :allocation #+lispworks :static))
         )
    (loop for i of-type fixnum from 0 below npoints
          for theta = (* i astep)
          for r = (* b (cos (* npetals theta)))
          do (setf (aref x-vec i) (float (if polar theta (* r (cos theta))) 1.0f0)
                   (aref y-vec i) (float (if polar r (* r (sin theta))) 1.0f0)
                   ))
    (values x-vec y-vec npoints)))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
#-lispworks
(eval-when (:compile-toplevel :load-toplevel :execute)
  (ql:quickload '(:cl-cffi-gtk :float-features) :silent t))

#+gtk
(defun plt-render-gtk (plt cr)
  (declare (optimize (speed 3) (safety 1)))
  (let ((max-rcmds (%plt-render plt (cffi:null-pointer) 0)))
    (declare (type fixnum max-rcmds))
    (when (> max-rcmds 0)
      (flet ((parse-color (color)
               (when (stringp color)
                 (let ((rgba (gdk:gdk-rgba-parse color)))
                   (when rgba
                     (list (gdk:gdk-rgba-red rgba)
                           (gdk:gdk-rgba-green rgba)
                           (gdk:gdk-rgba-blue rgba)
                           (gdk:gdk-rgba-alpha rgba)))))))

        (cffi:with-foreign-object (ptr-rcmds '(:struct plt-render-cmd) max-rcmds)
          (let ((count (%plt-render plt ptr-rcmds max-rcmds)))
            (declare (type fixnum count))
            (when (> count 0)
              (let ((struct-size (cffi:foreign-type-size '(:struct plt-render-cmd)))
                    (cur-x 0.0f0)
                    (cur-y 0.0f0)
                    (pen-style '(1)))
                (declare (type fixnum struct-size)
                         (type single-float cur-x cur-y))
                (cairo:cairo-save cr)
                (dotimes (i count)
                  (let* ((rcmd (cffi:inc-pointer ptr-rcmds (the fixnum (* i struct-size))))
                         (cmd  (cffi:foreign-slot-value rcmd '(:struct plt-render-cmd) 'cmd))
                         (op   (cmd-op cmd))
                         (x    (cffi:foreign-slot-value rcmd '(:struct plt-render-cmd) 'x))
                         (y    (cffi:foreign-slot-value rcmd '(:struct plt-render-cmd) 'y))
                         (arg1 (cffi:foreign-slot-value rcmd '(:struct plt-render-cmd) 'arg1))
                         (arg2 (cffi:foreign-slot-value rcmd '(:struct plt-render-cmd) 'arg2)))
                    (declare (type single-float x y arg1 arg2))
                      ;(format t "~A ~A ~A ~A ~A ~A \"~A\"~%" (cmd-z-index cmd) op x y arg1 arg2 (cmd-arg3 rcmd))
                    (case op
                      (#.+op-set-pen-color+
                       (let ((color (parse-color (cmd-arg3 rcmd))))
                         (when color
                           (apply #'cairo:cairo-set-source-rgba cr color))))

                      (#.+op-set-pen-style+
                       (let ((style (parse-style (cmd-arg3 rcmd))))
                         (setf pen-style style)
                         (when style
                           (cairo:cairo-set-dash cr (if (/= 1 (length style)) style nil) 0))))

                      (#.+op-set-pen-width+
                       (cairo:cairo-set-line-width cr arg1))

                      (#.+op-moveto+
                       (setf cur-x x
                             cur-y y))

                      (#.+op-lineto+
                       (when pen-style
                         (cairo:cairo-move-to cr cur-x cur-y)
                         (cairo:cairo-line-to cr x y)
                         (cairo:cairo-stroke cr))
                       (setf cur-x x
                             cur-y y))

                      (#.+op-draw-text+
                       (let ((text (cmd-arg3 rcmd)))
                         (when (> (length text) 0)
                           (let* ((angle     arg1)
                                  (alignment (truncate arg2))
                                  (extents   (cairo:cairo-text-extents cr text))
                                  (width     (cairo:cairo-text-extents-t-width extents))
                                  (x-bearing (cairo:cairo-text-extents-t-x-bearing extents))
                                  (y-bearing (cairo:cairo-text-extents-t-y-bearing extents))
                                  (height    (cairo:cairo-text-extents-t-height extents))
                                  (ox (cond
                                        ((plusp (logand alignment +align-hcenter+)) (/ width 2.0))
                                        ((plusp (logand alignment +align-right+))   width)
                                        (t                                          0.0)))
                                  (oy (cond
                                        ((plusp (logand alignment +align-vcenter+))  (+ y-bearing (/ height 2.0)))
                                        ((plusp (logand alignment +align-bottom+))   (+ y-bearing height))
                                        ((plusp (logand alignment +align-baseline+)) 0.0)
                                        (t                                           y-bearing))))
                             (cairo:cairo-save cr)
                             (cairo:cairo-translate cr x y)
                             (unless (zerop angle)
                               (cairo:cairo-rotate cr (- angle)))
                             (cairo:cairo-move-to cr (- (- ox) x-bearing) (- oy))
                             (cairo:cairo-show-text cr text)
                             (cairo:cairo-restore cr)))))

                      (#.+op-draw-ellipse+
                       (let ((fill-color (parse-color (cmd-arg3 rcmd)))
                             (rx arg1)
                             (ry arg2))
                         (when (or fill-color pen-style)
                           (cairo:cairo-save cr)
                           (cairo:cairo-translate cr x y)
                           (cairo:cairo-new-sub-path cr)
                           (cairo:cairo-scale cr rx ry)
                           (cairo:cairo-arc cr 0.0 0.0 1.0 0.0 (* 2.0 pi))
                           (when fill-color
                             (apply #'cairo:cairo-set-source-rgba cr fill-color)
                             (if pen-style
                                 (cairo:cairo-fill-preserve cr)
                                 (cairo:cairo-fill cr)))
                           (cairo:cairo-restore cr)
                           (when pen-style
                             (cairo:cairo-stroke cr)))))

                      (#.+op-draw-rect+
                       (let ((fill-color (parse-color (cmd-arg3 rcmd))))
                         (when (or fill-color pen-style)
                           (cairo:cairo-rectangle cr x y arg1 arg2)
                           (when fill-color
                             (cairo:cairo-save cr)
                             (apply #'cairo:cairo-set-source-rgba cr fill-color)
                             (if pen-style
                                 (cairo:cairo-fill-preserve cr)
                                 (cairo:cairo-fill cr))
                             (cairo:cairo-restore cr))
                           (when pen-style
                             (cairo:cairo-stroke cr)))))

                      (t (error "unknown op=~A~%" op)))))
                (cairo:cairo-restore cr)))))))))

#+capi
(defun plt-render-capi (plt pane)
  (declare (optimize (speed 3) (safety 1)))
  (let ((max-rcmds (%plt-render plt (cffi:null-pointer) 0)))
    (declare (type fixnum max-rcmds))
    (when (> max-rcmds 0)
      (flet ((parse-color (color)
               (when (stringp color)
                 (let ((color (string-trim " \t" color)))
                   (unless (string= color "")
                     (let ((has-hash (char= (char color 0) #\#)))
                       (or (and (not has-hash)
                                (let ((sym (find-symbol (string-upcase color) :keyword)))
                                  (when sym (color:get-color-spec sym))))
                           (let ((hex-str (if has-hash (subseq color 1) color)))
                             (when (and (every (lambda (c) (digit-char-p c 16)) hex-str)
                                        (or (= (length hex-str) 3) (= (length hex-str) 6)))
                               (if (= (length hex-str) 6)
                                   (let ((r (/ (parse-integer hex-str :start 0 :end 2 :radix 16) 255.0s0))
                                         (g (/ (parse-integer hex-str :start 2 :end 4 :radix 16) 255.0s0))
                                         (b (/ (parse-integer hex-str :start 4 :end 6 :radix 16) 255.0s0)))
                                     (color:make-rgb r g b))
                                   (let ((r (/ (parse-integer hex-str :start 0 :end 1 :radix 16) 15.0s0))
                                         (g (/ (parse-integer hex-str :start 1 :end 2 :radix 16) 15.0s0))
                                         (b (/ (parse-integer hex-str :start 2 :end 3 :radix 16) 15.0s0)))
                                     (color:make-rgb r g b))))))))))))

        (cffi:with-foreign-object (ptr-rcmds '(:struct plt-render-cmd) max-rcmds)
          (let ((count (%plt-render plt ptr-rcmds max-rcmds)))
            (declare (type fixnum count))
            (when (> count 0)
              (let ((struct-size (cffi:foreign-type-size '(:struct plt-render-cmd)))
                    (cur-x 0.0f0)
                    (cur-y 0.0f0)
                    (pen-style '(1)))
                (declare (type fixnum struct-size)
                         (type single-float cur-x cur-y))
                (gp:with-graphics-state (pane)
                  (dotimes (i count)
                    (let* ((rcmd (cffi:inc-pointer ptr-rcmds (the fixnum (* i struct-size))))
                           (cmd  (cffi:foreign-slot-value rcmd '(:struct plt-render-cmd) 'cmd))
                           (op   (cmd-op cmd))
                           (x    (cffi:foreign-slot-value rcmd '(:struct plt-render-cmd) 'x))
                           (y    (cffi:foreign-slot-value rcmd '(:struct plt-render-cmd) 'y))
                           (arg1 (cffi:foreign-slot-value rcmd '(:struct plt-render-cmd) 'arg1))
                           (arg2 (cffi:foreign-slot-value rcmd '(:struct plt-render-cmd) 'arg2))
                           )
                      (declare (type single-float x y arg1 arg2))
                      ;(format t "~A ~A ~A ~A ~A ~A \"~A\"~%" (cmd-z-index cmd) op x y arg1 arg2 (cmd-arg3 rcmd))
                      (case op
                        (#.+op-set-pen-color+
                         (let ((color (parse-color (cmd-arg3 rcmd))))
                           (when color
                             (gp:set-graphics-state pane :foreground color))))

                      (#.+op-set-pen-style+
                       (let ((style (parse-style (cmd-arg3 rcmd))))
                         (setf pen-style style)
                         (when style
                           (gp:set-graphics-state pane :dashed (/= 1 (length style)) :dash style))))

                        (#.+op-set-pen-width+
                         (gp:set-graphics-state pane :thickness arg1))

                        (#.+op-moveto+
                         (setf cur-x x
                               cur-y y))

                        (#.+op-lineto+
                         (when pen-style
                           (gp:draw-line pane
                                         (round cur-x) (round cur-y)
                                         (round x) (round y)))
                         (setf cur-x x
                               cur-y y))

                        (#.+op-draw-text+
                         (let* ((align-flags (truncate arg2))
                                (h-align (cond ((logtest align-flags +align-hcenter+) :center)
                                               ((logtest align-flags +align-right+)   :right)
                                               (t :left)))
                                (v-align (cond ((logtest align-flags +align-vcenter+)  :center)
                                               ((logtest align-flags +align-bottom+)   :bottom)
                                               ((logtest align-flags +align-baseline+) :baseline)
                                               (t :top)))
                                (text (cmd-arg3 rcmd)))
                           (when (> (length text) 0)
                             (flet ((draw-text ()
                                      (multiple-value-bind (left top right bottom)
                                        (gp:get-string-extent pane text)
                                        (let* ((width  (- right left))
                                               (height (- bottom top))
                                               (dx (case h-align
                                                     (:center (- (/ width 2) left))
                                                     (:right  (- width left))
                                                     (t       (- left))))
                                               (dy (case v-align
                                                     (:center   (+ (/ height 2) bottom))
                                                     (:bottom   (- bottom))
                                                     (:baseline 0)
                                                     (t         (- top)))))
                                          (gp:draw-string pane text (- x dx) (+ y dy))))))
                               (if (zerop arg1)
                                   (draw-text)
                                   (gp:with-graphics-transform (pane (gp:apply-rotation-around-point
                                                                       (gp:make-transform) (- arg1) x y))
                                     (draw-text)))))))

                        (#.+op-draw-ellipse+
                         (let ((fill-color (parse-color (cmd-arg3 rcmd))))
                           (when fill-color
                             (gp:draw-ellipse pane x y arg1 arg2 :foreground fill-color :filled t))
                           (when pen-style
                             (gp:draw-ellipse pane x y arg1 arg2 :filled nil))))

                        (#.+op-draw-rect+
                         (let ((fill-color (parse-color (cmd-arg3 rcmd))))
                           (when fill-color
                             (gp:draw-rectangle pane x y arg1 arg2 :foreground fill-color :filled t))
                           (when pen-style
                             (gp:draw-rectangle pane x y arg1 arg2 :filled nil))))

                        (t (error "unknown op=~A~%" op))))
                    ))))))))))

(defun plt-render (plt dc)
  #+capi
  (plt-render-capi plt dc)
  #+gtk
  (plt-render-gtk plt dc)
  )

#+gtk
(defun demo-gtk (&optional (coord +coord-cartesian+))
  (cffi:use-foreign-library libplot)
  (float-features:with-float-traps-masked (:invalid :divide-by-zero :overflow :underflow :inexact)
    (gtk:within-main-loop
      (let ((window (make-instance 'gtk:gtk-window
                                   :type :toplevel
                                   :title "Libplot demo"
                                   :default-width 800
                                   :default-height 600
                                   ))
            (drawing-area (make-instance 'gtk:gtk-drawing-area))
            (plot (plt-create coord))
            (curves nil)
            (last-x 0.0)
            (last-y 0.0)
            )
        (when (cffi:null-pointer-p plot)
          (setf plot nil)
          (error "Failed to create a plot"))

        (when plot
          (plt-set-option plot "plot.title" "Rose curve")
          (plt-set-option plot "plot.title.color" "red")
          (plt-set-option plot "plot.background.color" "#aaaaaa")
          (plt-set-option plot "plot.axisX.label" "Frequency")
          (plt-set-option plot "plot.axisX.unit" "Hz")
          (plt-set-option plot "plot.axisY.label" "a.u."))

        (glib:g-timeout-add 1000
          (lambda ()
            (when plot
              (multiple-value-bind (x-vec y-vec count)
                ;(archimedean-spiral
                (rose-curve
                  (+ 3 (random 3))
                  :polar (/= coord +coord-cartesian+))
                (if (> (length curves) 0)
                    (plt-curve (first curves) x-vec y-vec count)
                    (let ((crv (plt-curve plot x-vec y-vec count)))
                      (unless (cffi:null-pointer-p crv)
                        (plt-set-option crv "curve.color" "#0000FF")
                        (plt-set-option crv "curve.width" "2")
                        (plt-set-option crv "curve.marker.color" "#FF0000")
                        (plt-set-option crv "curve.marker.shape" "s")
                        (push crv curves))))
                (gtk:gtk-widget-queue-draw drawing-area)))
            nil ; don't repeat
            ))

        (gtk:gtk-widget-add-events drawing-area '(:pointer-motion-mask
                                                   :button-press-mask
                                                   :button-release-mask
                                                   :button1-motion-mask
                                                   :scroll-mask
                                                   :key-press-mask))

        (g:g-signal-connect window "destroy"
                            (lambda (widget)
                              (declare (ignore widget))
                              (when plot
                                (plt-destroy plot))
                              (cffi:close-foreign-library 'libplot)
                              (gtk:leave-gtk-main)))

        (g:g-signal-connect drawing-area "draw"
                            (lambda (widget cr)
                              (declare (ignore widget))
                              (when plot
                                (let ((ptr (gobject:pointer cr)))
                                  (plt-render plot ptr))
                                gdk:+gdk-event-stop+)))

        (g:g-signal-connect drawing-area "configure-event"
                            (lambda (widget event)
                              (declare (ignore widget))
                              (when plot
                                (let* ((width (gdk:gdk-event-configure-width event))
                                       (height (gdk:gdk-event-configure-height event)))
                                  (let ((size-str (format nil "~dx~d" width height)))
                                    (plt-set-option plot "plot.size" size-str)
                                    (gtk:gtk-widget-queue-draw drawing-area)))
                                t)))

        (g:g-signal-connect drawing-area "scroll-event"
                            (lambda (widget event)
                              (when plot
                                (let ((scroll-state (gdk:gdk-event-scroll-state event)))
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
                                        (gtk:gtk-widget-queue-draw widget)
                                        t)))))))

        (g:g-signal-connect drawing-area "motion-notify-event"
                            (lambda (widget event)
                              (declare (ignore widget))
                              (when plot
                                (let* ((x (gdk:gdk-event-motion-x event))
                                       (y (gdk:gdk-event-motion-y event)))
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
                                    (let* ((dx (- x last-x))
                                           (dy (- y last-y)))
                                      (plt-pan plot (float dx 1.0f0) (float dy 1.0f0))
                                      (setf last-x x
                                            last-y y)
                                      (gtk:gtk-widget-queue-draw drawing-area)))
                                  t))))

        (g:g-signal-connect drawing-area "button-press-event"
                            (lambda (widget event)
                              (declare (ignore widget))
                              (when (= (gdk:gdk-event-button-button event) 1)
                                (setf last-x (gdk:gdk-event-button-x event)
                                      last-y (gdk:gdk-event-button-y event)))
                              t))

        (gtk:gtk-container-add window drawing-area)
        (gtk:gtk-widget-show-all window)))))

#+capi
(capi:define-interface main-interface ()
  ((plt-coord  :initform +coord-cartesian+ :initarg :coord :accessor window-plt-coord)
   (plt-handle :initform nil :accessor window-plt-handle)
   (plt-curves :initform nil :accessor window-plt-curves)
   (timer      :initform nil :accessor window-timer)
   (last-x     :initform 0   :accessor window-last-x)
   (last-y     :initform 0   :accessor window-last-y)
   )

  (:panes
   (plot-pane capi:output-pane
              :draw-with-buffer t
              :create-callback (lambda (pane)
                                 (let ((interface (capi:element-interface pane)))
                                   (cffi:use-foreign-library libplot)
                                   (let ((plot (plt-create (window-plt-coord interface))))
                                     (if (cffi:null-pointer-p plot)
                                         (error "Failed to create a plot")
                                         (progn
                                           (setf (window-plt-handle interface) plot)
                                           (plt-set-option plot "plot.title" "Rose curve")
                                           (plt-set-option plot "plot.title.color" "red")
                                           (plt-set-option plot "plot.background.color" "#aaaaaa")
                                           (plt-set-option plot "plot.font.size"
                                                           (format nil "~Ax~A"
                                                                   ;(multiple-value-bind (left top right bottom) (gp:get-character-extent pane #\W) (- right left))
                                                                   (gp:get-font-average-width pane)
                                                                   (gp:get-font-height pane)))
                                           (plt-set-option plot "plot.axisX.label" "Frequency")
                                           (plt-set-option plot "plot.axisY.label" "a.u.")

                                           (let ((timer (mp:make-timer
                                                          (lambda (interface)
                                                            (let ((pane (slot-value interface 'plot-pane)))
                                                              (capi:apply-in-pane-process
                                                                pane
                                                                (lambda ()
                                                                  (let ((plot (window-plt-handle interface))
                                                                        (crvs (window-plt-curves interface)))
                                                                    (unless (or (null plot) (cffi:null-pointer-p plot))
                                                                      (multiple-value-bind (x-vec y-vec count)
                                                                        ;(archimedean-spiral
                                                                        (rose-curve
                                                                          (+ 3 (random 3))
                                                                          :polar (/= (window-plt-coord interface) +coord-cartesian+))
                                                                        (if (> (length crvs) 0)
                                                                            (plt-curve (first crvs) x-vec y-vec count)
                                                                            (let ((crv (plt-curve plot x-vec y-vec count)))
                                                                              (unless (cffi:null-pointer-p crv)
                                                                                (plt-set-option crv "curve.color" "#0000FF")
                                                                                (plt-set-option crv "curve.width" "2")
                                                                                (plt-set-option crv "curve.marker.color" "#FF0000")
                                                                                (plt-set-option crv "curve.marker.shape" "s")
                                                                                (push crv crvs)
                                                                                (setf (window-plt-curves interface) crvs)
                                                                                )))

                                                                        (gp:invalidate-rectangle pane))))

                                                                  ; kill the timer
                                                                  (when t
                                                                    (let ((timer (window-timer interface)))
                                                                      (when timer
                                                                        (mp:unschedule-timer timer)
                                                                        (setf (window-timer interface) nil)))
                                                                    )))))
                                                          interface)))
                                             (setf (window-timer interface) timer)
                                             (mp:schedule-timer-relative timer 1.0 1.0))
                                           )))))


              :destroy-callback (lambda (pane)
                                  (let* ((interface (capi:element-interface pane))
                                         (timer (window-timer interface))
                                         )
                                    (when timer
                                      (mp:unschedule-timer timer)
                                      (setf (window-timer interface) nil))

                                    (let ((plot (window-plt-handle interface)))
                                      (unless (or (null plot) (cffi:null-pointer-p plot))
                                        (plt-destroy plot)
                                        (setf (window-plt-handle interface) nil)))

                                    (cffi:close-foreign-library 'libplot)
                                    ))

              :resize-callback (lambda (pane x y width height)
                                 (declare (ignore x y))
                                 (let* ((interface (capi:element-interface pane))
                                        (plot (window-plt-handle interface)))
                                   (unless (or (null plot) (cffi:null-pointer-p plot))
                                     (let ((size-str (format nil "~dx~d" width height)))
                                       (plt-set-option plot "plot.size" size-str)
                                       (gp:invalidate-rectangle pane)))))

              :display-callback (lambda (pane x y width height)
                                  (declare (ignore x y width height))
                                  (let* ((interface (capi:element-interface pane))
                                         (plot (window-plt-handle interface)))
                                    (unless (or (null plot) (cffi:null-pointer-p plot))
                                      (plt-render plot pane))))

              :input-model (list
                             (list :motion
                               (lambda (pane x y)
                                 (let* ((interface (capi:element-interface pane))
                                        (plot (window-plt-handle interface)))

                                  (cffi:with-foreign-objects ((x-ref :float)
                                                              (y-ref :float)
                                                              (y2-ref :float))
                                    (plt-pix2math plot (float x 1.0f0) (float y 1.0f0) x-ref y-ref y2-ref)
                                    (let ((lx (cffi:mem-ref x-ref :float))
                                          (ly (cffi:mem-ref y-ref :float))
                                          (ly2 (cffi:mem-ref y2-ref :float)))
                                      (setf (capi:interface-title interface)
                                            (format nil "Libplot demo: (~A, ~A) -> (~A, ~A, ~A)" x y lx ly ly2)))
                                   ))))

                             (list '(:button-1 :press)
                               (lambda (pane x y)
                                 (let ((interface (capi:element-interface pane)))
                                   (setf (window-last-x interface) x)
                                   (setf (window-last-y interface) y)
                                   )))

                             (list '(:button-1 :motion)
                               (lambda (pane x y)
                                 (let* ((interface (capi:element-interface pane))
                                        (plot (window-plt-handle interface))
                                        (input-model (capi:output-pane-input-model pane))
                                        (entry (assoc '(:touch :pan) input-model :test #'equal)))

                                   (when entry
                                     (destructuring-bind (fn &rest extra-args) (cdr entry)
                                       (apply fn pane x y
                                              (- x (window-last-x interface))
                                              (- y (window-last-y interface)) extra-args)))

                                   (setf (window-last-x interface) x)
                                   (setf (window-last-y interface) y)
                                   )))

                             (list '(:touch :pan)
                               (lambda (pane x y dx dy)
                                 (let* ((interface (capi:element-interface pane))
                                        (plot (window-plt-handle interface)))
                                   (plt-pan plot (float dx) (float dy))
                                   (gp:invalidate-rectangle pane))))

                             (list '(:touch :zoom)
                               (lambda (pane x y factor)
                                 (let* ((interface (capi:element-interface pane))
                                        (plot (window-plt-handle interface)))
                                   (plt-zoom plot (float x) (float y) factor)
                                   (gp:invalidate-rectangle pane))))
                             )))

  (:layouts
   (main-layout capi:column-layout
                (list 'plot-pane)))

  (:default-initargs
   :layout 'main-layout
   :width 800
   :height 600
   :title "Libplot demo"))

(defun demo (&optional (coord +coord-cartesian+))
  #+capi
  (capi:display (make-instance 'main-interface :coord coord))
  #+gtk
  (demo-gtk coord))
