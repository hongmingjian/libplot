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
           #:demo
           ))
(in-package :libplot)

(fli:define-c-struct plt-render-cmd
  (cmd  :unsigned-int)
  (x    :float)
  (y    :float)
  (arg1 :float)
  (arg2 :float)
  (arg3 (:ef-mb-string :limit 64)))

(declaim (inline cmd-op cmd-z-index))
(defun cmd-op (cmd)      (ldb (byte 8 0) cmd))
(defun cmd-z-index (cmd) (ldb (byte 8 8) cmd))

(defconstant +coord-cartesian+ 0)
(defconstant +coord-polar+ 1)

(defconstant +op-set-text-color+ 1)
(defconstant +op-draw-text+      2)
(defconstant +op-set-line-color+ 3)
(defconstant +op-set-line-style+ 4)
(defconstant +op-set-line-width+ 5)
(defconstant +op-moveto+         6)
(defconstant +op-lineto+         7)
(defconstant +op-draw-ellipse+   8)
(defconstant +op-draw-rect+      9)

(defconstant +align-left+     #x00000000)
(defconstant +align-hcenter+  #x00000001)
(defconstant +align-right+    #x00000002)
(defconstant +align-top+      #x00000000)
(defconstant +align-vcenter+  #x00010000)
(defconstant +align-bottom+   #x00020000)
(defconstant +align-baseline+ #x00040000)

(fli:define-foreign-function (plt-create "plt_create")
    ((coord :int))
    :result-type :pointer
    :module "libplot"
  )

(fli:define-foreign-function (plt-destroy "plt_destroy")
    ((plt :pointer))
    :result-type :void
    :module "libplot"
  )

(fli:define-foreign-function (plt-set-option "plt_set_option")
    ((handle :pointer)
     (option (:reference-pass :ef-mb-string))
     (value  (:reference-pass :ef-mb-string)))
  :result-type :int
  :module "libplot"
  )

(fli:define-foreign-function (plt-pan "plt_pan")
    ((plt :pointer)
     (dx :float)
     (dy :float))
  :result-type :void
  :module "libplot"
  )

(fli:define-foreign-function (plt-zoom "plt_zoom")
    ((plt :pointer)
     (cx :float)
     (cy :float)
     (factor :float))
  :result-type :void
  :module "libplot"
  )

(fli:define-foreign-function (plt-pix2math "plt_pix2math")
    ((plt :pointer)
     (px :float)
     (py :float)
     (x (:reference-return :float))
     (y (:reference-return :float))
     (y2 (:reference-return :float)))
  :result-type :void
  :module "libplot"
  )

(fli:define-foreign-function (%plt-curve "plt_curve")
    ((handle :pointer)
     (x :pointer)
     (y :pointer)
     (count :int))
  :result-type :pointer
  :module "libplot"
  )
(defun plt-curve (handle x y count)
  (if (zerop count)
      (%plt-curve handle fli:*null-pointer* fli:*null-pointer* count)
      (fli:with-dynamic-lisp-array-pointer (x-ptr x)
          (fli:with-dynamic-lisp-array-pointer (y-ptr y)
              (%plt-curve handle x-ptr y-ptr count)))))

(fli:define-foreign-function (%plt-append "plt_append")
    ((crv :pointer)
     (x (:pointer :float))
     (y (:pointer :float))
     (count :int))
  :result-type :void
  :module "libplot"
  )
(defun plt-append (crv x y count)
  (fli:with-dynamic-lisp-array-pointer (x-ptr x)
      (fli:with-dynamic-lisp-array-pointer (y-ptr y)
          (%plt-append crv x-ptr y-ptr count))))

(fli:define-foreign-function (plt-remove "plt_remove")
    ((crv :pointer))
  :result-type :void
  :module "libplot"
  )

(fli:define-foreign-function (plt-render "plt_render")
    ((plt :pointer)
     (rcmds (:pointer (:struct plt-render-cmd)))
     (count :int))
  :result-type :int
  :module "libplot"
  )

(defun parse-color (input)
  (cond
    ((keywordp input) input)

    ((symbolp input) (intern (symbol-name input) :keyword))

    ((stringp input)
     (let* ((s (string-trim " \t\n" input))
            (has-hash (and (> (length s) 0) (char= (char s 0) #\#)))
            (hex-str (if has-hash (subseq s 1) s)))

       (if (and (every (lambda (c) (digit-char-p c 16)) hex-str)
                (or (= (length hex-str) 3) (= (length hex-str) 6)))

           (if (= (length hex-str) 6)
               (let ((r (/ (parse-integer hex-str :start 0 :end 2 :radix 16) 255.0))
                     (g (/ (parse-integer hex-str :start 2 :end 4 :radix 16) 255.0))
                     (b (/ (parse-integer hex-str :start 4 :end 6 :radix 16) 255.0)))
                 (color:make-rgb (coerce r 'short-float)
                                 (coerce g 'short-float)
                                 (coerce b 'short-float)))
               (let ((r (/ (parse-integer (string (char hex-str 0)) :radix 16) 15.0))
                     (g (/ (parse-integer (string (char hex-str 1)) :radix 16) 15.0))
                     (b (/ (parse-integer (string (char hex-str 2)) :radix 16) 15.0)))
                 (color:make-rgb (coerce r 'short-float)
                                 (coerce g 'short-float)
                                 (coerce b 'short-float))))

           (let ((sym (intern (string-upcase s) :keyword)))
             (if (color:get-color-spec sym)
                 sym
                 nil
                 )))))
    (t
      nil
      )))

(defun parse-style (style)
  (when (stringp style)
    (let ((s (string-trim " \t\r\n" style)))
      (cond
        ((string= s "-") '(1)) ; solid
        ((string= s "--") '(6 4)) ; dash
        ((string= s ":") '(2 2)) ; dot
        ((string= s "-.") '(6 3 2 3)) ; dashdot
        ((string= s "-..") '(6 3 2 3 2 3)) ;dashdotdot
        (t nil) ; = PS_NULL in GDI
        ))))

(defun draw-string (pane string x y &key (h-align :left) (v-align :top))
  "h-align: :left, :center, :right
   v-align: :baseline, :top, :center, :bottom"
  (multiple-value-bind (left top right bottom)
      (gp:get-string-extent pane string)
    (let* ((width  (- right left))
           (height (- bottom top))
           (dx (case h-align
                 (:center (- (/ width 2) left))
                 (:right  (- width left))
                 (t       (- left)) ; :left
                 ))
           (dy (case v-align
                 (:center   (+ (/ height 2) bottom))
                 (:bottom   (- bottom))
                 (:baseline 0)
                 (t         (- top)) ; :top
                 )))
      (gp:draw-string pane string (- x dx) (+ y dy)))))

(defun execute-render-cmd (pane rcmd draw-state)
  (let* ((cmd    (fli:foreign-slot-value rcmd 'cmd))
         (op     (cmd-op cmd))
         (x      (fli:foreign-slot-value rcmd 'x))
         (y      (fli:foreign-slot-value rcmd 'y))
         (arg1   (fli:foreign-slot-value rcmd 'arg1))
         (arg2   (fli:foreign-slot-value rcmd 'arg2))
         (arg3   (fli:foreign-slot-value rcmd 'arg3)))

;    (format t "~A ~A ~A ~A ~A ~A~%" op x y arg1 arg2 arg3)
    (case op
      (#.+op-set-text-color+
       (let ((color (parse-color arg3)))
         (unless (null color)
           (setf (getf draw-state :text-color) color))))

      (#.+op-draw-text+
       (let* ((align-flags (truncate arg2))
              (h-align (cond ((logtest align-flags +align-hcenter+) :center)
                             ((logtest align-flags +align-right+) :right)
                             (t :left)))
              (v-align (cond ((logtest align-flags +align-vcenter+) :center)
                             ((logtest align-flags +align-bottom+) :bottom)
                             ((logtest align-flags +align-baseline+) :baseline)
                             (t :top))))

         (gp:with-graphics-state (pane :foreground (getf draw-state :text-color))
           (gp:with-graphics-transform (pane (gp:apply-rotation-around-point
                                               (gp:make-transform) (- arg1) x y))
             (draw-string pane arg3 x y :h-align h-align :v-align v-align)))))

      (#.+op-set-line-color+
       (let ((color (parse-color arg3)))
         (unless (null color)
           (setf (getf draw-state :line-color) color))))

      (#.+op-set-line-style+
       (let ((style (parse-style arg3)))
         (setf (getf draw-state :line-style) style)))

      (#.+op-set-line-width+
       (setf (getf draw-state :line-width) arg1))

      (#.+op-moveto+
       (setf (getf draw-state :x) x
             (getf draw-state :y) y))

      (#.+op-lineto+
       (let ((cur-x (getf draw-state :x))
             (cur-y (getf draw-state :y))
             (color (getf draw-state :line-color))
             (style (getf draw-state :line-style))
             (width (getf draw-state :line-width)))
         (unless (null style)
           (gp:with-graphics-state (pane :foreground color
                                         :thickness (max 1 (round width))
                                         :dashed (/= 1 (length style))
                                         :dash style
                                         )
                                   (gp:draw-line pane
                                                 (round cur-x) (round cur-y)
                                                 (round x) (round y))))
         (setf (getf draw-state :x) x
               (getf draw-state :y) y)))

      (#.+op-draw-ellipse+
       (let ((border-color (getf draw-state :line-color))
             (fill-color (parse-color arg3)))
         (unless (null fill-color)
           (gp:with-graphics-state (pane :foreground fill-color)
                                   (gp:draw-ellipse pane x y arg1 arg2 :filled t)))
         (gp:with-graphics-state (pane :foreground border-color)
                                 (gp:draw-ellipse pane x y arg1 arg2 :filled nil))))

      (#.+op-draw-rect+
       (let ((border-color (getf draw-state :line-color))
             (fill-color (parse-color arg3)))
         (unless (null fill-color)
           (gp:with-graphics-state (pane :foreground fill-color)
                                   (gp:draw-rectangle pane x y arg1 arg2 :filled t)))
         (gp:with-graphics-state (pane :foreground border-color)
                                 (gp:draw-rectangle pane x y arg1 arg2 :filled nil))))


      (t nil)
      )))

(defun execute-render-cmds (pane ptr-rcmds count)
  (when (> count 0)
    (let ((indices (loop for i from 0 below count collect i)))

      (setf indices
            (stable-sort indices #'<
                         :key (lambda (i)
                                (let* ((ptr-rcmd (fli:incf-pointer (fli:copy-pointer ptr-rcmds) i))
                                       (cmd (fli:foreign-slot-value ptr-rcmd 'cmd)))
                                  (cmd-z-index cmd)))))

      (let ((draw-state (list :x 0.0 :y 0.0
                              :line-color :black
                              :line-style nil
                              :line-width 1.0
                              :text-color :black))
            (ptr-rcmd (fli:copy-pointer ptr-rcmds)))
        (dolist (i indices)
          (execute-render-cmd pane ptr-rcmd draw-state)
          (fli:incf-pointer ptr-rcmd))))))

;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;;
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
              :display-callback 'on-display-pane
              :create-callback 'on-create-pane
              :destroy-callback 'on-destroy-pane
              :resize-callback 'on-resize-pane
              :input-model '((:motion             on-mouse-move)
                             ((:button-1 :press)  on-mouse-press)
                             ((:button-1 :motion) on-mouse-drag)
                             ((:touch :zoom)      on-zoom)
                             ((:touch :pan)       on-pan)
                             )
              ))

  (:layouts
   (main-layout capi:column-layout
                (list 'plot-pane)))

  (:default-initargs
   :layout 'main-layout
   :width 800
   :height 600
   :title "Libplot demo"))

(defun archimedean-spiral (nturns points-per-turn &key (polar nil))
  (let* ((npoints (1+ (* nturns points-per-turn)))
         (x-vec (make-array npoints :element-type 'single-float :allocation :static))
         (y-vec (make-array npoints :element-type 'single-float :allocation :static))
         (a 0)
         (b 1)
         )
    (loop for i from 0 below npoints
          for theta = (* i (/ (* 2.0 pi) points-per-turn))
          for r = (+ a (* b theta))
          do (setf (aref x-vec i) (coerce (if polar
                                              (rem theta (* 2.0 pi))
                                              (* r (cos theta)))
                                          'single-float)
                   (aref y-vec i) (coerce (if polar
                                              r
                                              (* r (sin theta)))
                                          'single-float)))
    (values x-vec y-vec npoints)))

(defun on-create-pane (pane)
  (let ((interface (capi:element-interface pane)))
    (fli:register-module "libplot"
                         :real-name "Z:\\hmj\\Sourcery\\libplot\\Debug\\libplot.dll"
                         )
    (let ((plot (plt-create (window-plt-coord interface))))
      (if (fli:null-pointer-p plot)
          (error "Failed to create a plot")
          (progn
            (setf (window-plt-handle interface) plot)
            (plt-set-option plot "plot.title" "Archimedean spiral")
            (plt-set-option plot "plot.background.color" "#aaaaaa")
            (plt-set-option plot "plot.font.size"
                            (format nil "~Ax~A"
                                    ;(multiple-value-bind (left top right bottom) (gp:get-character-extent pane #\W) (- right left))
                                    (gp:get-font-average-width pane)
                                    (gp:get-font-height pane)))
            (plt-set-option plot "plot.axisX.label" "Frequency")
            (plt-set-option plot "plot.axisY.label" "a.u.")

            (let ((timer (mp:make-timer 'on-tick-timer interface)))
              (setf (window-timer interface) timer)
              (mp:schedule-timer-relative timer 1.0 1.0))
            )))))

(defun on-destroy-pane (pane)
  (let* ((interface (capi:element-interface pane))
         (timer (window-timer interface))
         )
    (when timer
      (mp:unschedule-timer timer)
      (setf (window-timer interface) nil))

    (let ((plot (window-plt-handle interface)))
      (unless (or (null plot) (fli:null-pointer-p plot))
        (plt-destroy plot)
        (setf (window-plt-handle interface) nil)))

    (fli:disconnect-module "libplot" :remove t)
    ))

(defun on-display-pane (pane x y width height)
  (declare (ignore x y width height))
  (let* ((interface (capi:element-interface pane))
         (plot (window-plt-handle interface)))
    (unless (or (null plot) (fli:null-pointer-p plot))
      (let ((max-rcmds 1024))
        (fli:with-dynamic-foreign-objects ((ptr-rcmds (:struct plt-render-cmd) :nelems max-rcmds))
          (let ((num-rcmds (plt-render plot ptr-rcmds max-rcmds)))
            (execute-render-cmds pane ptr-rcmds num-rcmds)))))))

(defun on-resize-pane (pane x y width height)
  (declare (ignore x y))
  (let* ((interface (capi:element-interface pane))
         (plot (window-plt-handle interface)))
    (unless (or (null plot) (fli:null-pointer-p plot))
      (let ((size-str (format nil "~dx~d" width height)))
        (plt-set-option plot "plot.size" size-str)
        (gp:invalidate-rectangle pane)))))

(defun on-tick-timer (interface)
  (let ((pane (slot-value interface 'plot-pane)))
    (capi:apply-in-pane-process
     pane
     (lambda ()
       (let ((plot (window-plt-handle interface))
             (crvs (window-plt-curves interface)))
         (unless (or (null plot) (fli:null-pointer-p plot))
           (multiple-value-bind (x-vec y-vec count)
             (archimedean-spiral (+ 3 (random 3)) 72
                                 :polar (/= (window-plt-coord interface) +coord-cartesian+))
             (if (> (length crvs) 0)
                 (plt-curve (first crvs) x-vec y-vec count)
                 (let ((crv (plt-curve plot x-vec y-vec count)))
                   (unless (fli:null-pointer-p crv)
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
         )

       ))))

(defun on-mouse-move (pane x y)
  (let* ((interface (capi:element-interface pane))
         (plot (window-plt-handle interface)))
    (multiple-value-bind (lx ly ly2)
      (plt-pix2math plot (float x) (float y) t t t)
      (setf (capi:interface-title interface)
            (format nil "Libplot demo: (~A, ~A) -> (~A, ~A, ~A)" x y lx ly ly2)))
    ))

(defun on-mouse-press (pane x y)
  (let ((interface (capi:element-interface pane)))
    (setf (window-last-x interface) x)
    (setf (window-last-y interface) y)
  ))

(defun on-mouse-drag (pane x y)
  (let* ((interface (capi:element-interface pane))
         (plot (window-plt-handle interface)))
    (on-pan pane x y
             (- x (window-last-x interface))
             (- y (window-last-y interface)))
    (setf (window-last-x interface) x)
    (setf (window-last-y interface) y)
  ))

(defun on-pan (pane x y dx dy)
  (let* ((interface (capi:element-interface pane))
         (plot (window-plt-handle interface)))
    (plt-pan plot (float dx) (float dy))
    (gp:invalidate-rectangle pane)))

(defun on-zoom (pane x y factor)
  (let* ((interface (capi:element-interface pane))
         (plot (window-plt-handle interface)))
    (plt-zoom plot (float x) (float y) factor)
    (gp:invalidate-rectangle pane)))

(defun demo (&optional (coord +coord-cartesian+))
  (capi:display (make-instance 'main-interface :coord coord)))
