;;
;; Some handy elisp stuff to automagically generate descriptive
;;  SANE webpages from backend .desc files.
;;
;; Basic usage:
;;  M-x sane-desc-parse-directory          to parse .desc files in a directory
;;  M-x sane-desc-generate-backend-page    to write out some HTML
;; or, even easier,
;;  M-x sane-desc-regenerate-backend-page  do both of the above
;;
;;
;; Copyright (C) 1998 Matthew Marjanovic
;;
;; This program is free software; you can redistribute it and/or modify
;; it under the terms of the GNU General Public License as published by
;; the Free Software Foundation; either version 2, or (at your option)
;; any later version.
;;
;; This program is distributed in the hope that it will be useful,
;; but WITHOUT ANY WARRANTY; without even the implied warranty of
;; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
;; GNU General Public License for more details.
;;
;; You should have received a copy of the GNU General Public License
;; along with GNU Emacs; see the file COPYING.  If not, write to
;; the Free Software Foundation, 675 Massachusettes Ave,
;; Cambridge, MA 02139, USA.


(defconst sane-desc-descfile-regex ".*\.desc$"
  "Regular expression for finding .desc files.")

(defconst sane-desc-valid-devtypes '(:scanner :api :meta :stillcam :vidcam)
  "List of allowed device-type keywords")

(defvar sane-desc-status-codes
  '((:new    . ("NEW!"   "000080" 
		"means brand-new to the current release of SANE."))
    (:alpha  . ("alpha"  "bb0000" 
		"means it must do something, but is not very well
                 tested, probably has bugs, and may even crash your
                 system, etc., etc."))
    (:beta   . ("beta"   "806000" 
		"means it works pretty well, and looks stable and
                 functional, but not bullet-proof."))
    (:stable . ("stable" "008000" 
		"means someone is pulling your leg."))
    )
  "An assoc list with one pair for each valid :status keyword.
The associated data is a list with three elements, consisting of
the print name of the status, the color to use, and a description.")

;;; HTML formatting variables

(defvar sane-desc-backend-page-name "sane-backends.html"
  "Default name for the Backends HTML page.")


(defvar sane-desc-backend-html-title "Backends (Drivers)"
  "Title used for the Backends page.")

;(defvar sane-desc-base-url "http://www.mostang.com/sane/backends.html"
;  "Base URL for generated HTML.
;This is put into a BASE tag, and will be used as the root
;of all relative references.
;This will also be used for SANE home page references.")

(defvar sane-desc-home-url "http://www.mostang.com/sane/"
  "URL for the SANE homepage.")

(defvar sane-desc-sane-logo "http://www.mostang.com/sane/sane.gif"
  "URL for SANE logo.")

(defvar sane-desc-backend-html-bgcolor "FFFFFF"
  "Background color on Backends page.")

(defvar sane-desc-table-head-bgcolor "E0E0FF"
  "Background color for header of Backends table.")

(defvar sane-desc-email-address "sane-devel@mostang.com"
  "Address of the SANE developer's mailing list.")

(defvar sane-desc-manpage-url-format 
  "http://www.mostang.com/sane/man/%s.5.html"
  "Format used for creating URL's for on-line manpages.")



; why doesn't elisp have a normal format?  (beats me.)

(defmacro sane-desc-pformat (stream &rest args)
  `(princ (format ,@args) ,stream))



; structures for elements of the database

(defmacro sane-desc-getval (alist key)
  `(cdr (assq ,key ,alist)))

(defmacro sane-desc-putval (alist key val)
  `(setcdr (assq ,key ,alist) ,val))

(defmacro sane-desc-tack-on (lst item)
  `(setq ,lst (nconc ,lst (list ,item))))

(defmacro sane-desc-tack-on-val (alst key item)
  `(sane-desc-putval ,alst ,key
	   (nconc (sane-desc-getval ,alst ,key) (list ,item))))


(defun sane-desc-make-db ()
  (copy-alist '((:backends . nil)  ;why copy?  seems to be constant otherwise
		(:mfgs . nil)
		(:devices . nil))))

(defun sane-desc-make-backend ()
  (copy-alist '((:name . nil)     
		(:version . nil)
		(:status . nil)
		(:manpage . nil)
		(:url . nil)
		(:comment . nil)
		(:devlist . nil)
		(:mfglist . nil))))

(defun sane-desc-make-mfg ()
  (copy-alist '((:name . nil)
		(:url . nil)
		(:comment . nil)
		(:devlist . nil))))

(defun sane-desc-make-dev ()
  (copy-alist '((:type . nil)
		(:desc . nil)
		(:mfg . nil)
		(:url . nil)
		(:comment . nil))))


(defvar sane-desc-database (sane-desc-make-db)
  "The 'database' of results from parsing SANE .desc files.
This is a assoc list containing lists of backends, manufacturers,
and devices.")



(defun sane-desc-error-repeat (token)
  (error "Oops!  %s specified twice" token))


; add-to-list

(defun sane-desc-parse-file (filename)
  (interactive "fParse desc file: ")
  (let ((pbuff (find-file filename))
	result)
    (setq result (sane-desc-parse-buffer pbuff))
    (kill-buffer pbuff)
    result
    ))
      

(defun sane-desc-parse-buffer (pbuff-name)
  (interactive "bParse desc buffer: ")
  (let ((pbuff (get-buffer pbuff-name))
	(bk (sane-desc-make-backend))
	mfg-list
	dev-list
	current-devtype
	current-mfg
	token
	latest
	)
    (if (not (bufferp pbuff)) (error "No such buffer:  %s" pbuff-name))
    (save-excursion
      (set-buffer pbuff)
      (goto-char (point-min))
      ;;magic do loop -- read objects until EOF
      (while (condition-case nil
		 (progn 
		   (setq token (read pbuff))
		   t)
	       ((end-of-file nil))
	       )
	(cond
	 ;; Top-level backend descriptive elements
	 ;; ... :backend (required)
	 ((eq token :backend) 
	  (if (sane-desc-getval bk :name) (sane-desc-error-repeat token))
	  (sane-desc-putval bk :name (read pbuff))
	  (setq latest bk)
	  )
	 ;; ... :version
	 ((eq token :version) 
	  (if (sane-desc-getval bk :version) (sane-desc-error-repeat token))
	  (sane-desc-putval bk :version (read pbuff))
	  )
	 ;; ... :status
	 ((eq token :status) 
	  (if (sane-desc-getval bk :status) (sane-desc-error-repeat token))
	  (sane-desc-putval bk :status (read pbuff))
	  (if (not (assq (sane-desc-getval bk :status) sane-desc-status-codes))
	      (error "Not a valid status keyword: %s" (sane-desc-getval bk :status)))
	  )
	 ;; ... :manpage
	 ((eq token :manpage) 
	  (if (sane-desc-getval bk :manpage) (sane-desc-error-repeat token))
	  (sane-desc-putval bk :manpage (read pbuff))
	  )
	 ;; ... :devicetype -> signifies that device descriptions follow.
	 ((eq token :devicetype) 
	  (setq current-devtype (read pbuff))
	  (if (not (memq current-devtype sane-desc-valid-devtypes))
	      (error "Invalid device-type: %s" current-devtype))
	  )
	 ;; Device descriptive elements
	 ;; ... :mfg -> signal a new manufacturer, start a list
	 ((eq token :mfg)
	  (let ((name (read pbuff)))
	    (setq current-mfg (sane-desc-make-mfg))
	    (sane-desc-putval current-mfg :name name)
	    (setq mfg-list (nconc mfg-list (list current-mfg)))
	    (setq latest current-mfg)
	    ))
	 ;; ... :model -> name a model, assign to current mfg list
	 ((eq token :model)
	  (let ((name (read pbuff)))
	    (setq current-dev (sane-desc-make-dev))
	    (sane-desc-putval current-dev :desc name)
	    (sane-desc-putval current-dev :type current-devtype)
	    (if current-mfg
		(sane-desc-putval current-dev :mfg current-mfg)
	      (error "Device %s needs a mfg" name))
	    (sane-desc-tack-on dev-list current-dev)
	    (sane-desc-tack-on-val current-mfg :devlist current-dev)
	    (setq latest current-dev)
	    ))
	 ;; ... :desc ->  describe a software device (null mfg)
	 ((eq token :desc)
	  (let ((desc (read pbuff)))
	    (setq current-mfg (sane-desc-make-mfg))
	    (setq mfg-list (nconc mfg-list (list current-mfg)))
	    (setq current-dev (sane-desc-make-dev))
	    (sane-desc-putval current-dev :desc desc)
	    (sane-desc-putval current-dev :type current-devtype)
	    (sane-desc-putval current-dev :mfg current-mfg)
	    (sane-desc-tack-on dev-list current-dev)
	    (sane-desc-tack-on-val current-mfg :devlist current-dev)
	    (setq latest current-dev)
	    (setq current-mfg nil)
	    ))
	 ;; Extraneous elements (apply to latest object)
	 ;; ... :url
	 ((eq token :url)
	  (let ((url (read pbuff)))
	    (if latest
		(sane-desc-putval latest :url url)
	      (error "Assign :url %s to what?" url)
	      )))
	 ;; ... :comment
	 ((eq token :comment)
	  (let ((comment (read pbuff)))
	    (if latest
		(sane-desc-putval latest :comment comment)
	      (error "Assign :comment %s to what?" comment)
	      )))
	 ;; What could possibly be left???
	 (t (error "Unknown token during parse: %s" token))
	 ))
      (if (not (sane-desc-getval bk :name))
	  (error "Missing :backend specifier!"))
      (sane-desc-putval bk :devlist dev-list)
      (sane-desc-putval bk :mfglist mfg-list)
      (message "Parsed out:  %s (v%s, %s)" 
	       (sane-desc-getval bk :name) (sane-desc-getval bk :version) (sane-desc-getval bk :status))
      )
    (list bk mfg-list dev-list)
    ))


(defun sane-desc-parse-directory (dirname)
  (interactive "DParse all desc files in: ")
  (let ((files (directory-files dirname t sane-desc-descfile-regex))
	bklist
	mfglist
	devlist
	res
	(count 0))
    (setq bklist
	  (mapcar '(lambda (filename)
		     (message "Parsing %s" filename)
		     (setq res (sane-desc-parse-file filename))
		     (setq mfglist (append mfglist (nth 1 res)))
		     (setq devlist (append devlist (nth 2 res)))
		     (setq count (1+ count))
		     (nth 0 res) ; return the bk
		     )
		  files))
    (sane-desc-putval sane-desc-database :backends bklist)
    (sane-desc-putval sane-desc-database :mfgs mfglist)
    (sane-desc-putval sane-desc-database :devices devlist)
    (message "Parsed %d files" count)
    nil
    ))


;;;;----------------------------------------------------------


(defun sane-desc-generate-backend-page (filename)
  (interactive (list (read-file-name "Generate Backends HTML listing in: "
				     default-directory nil nil
				     sane-desc-backend-page-name)))
  (if (not (sane-desc-getval sane-desc-database :backends))
      (error "No backends in database!"))
(message "creating file %s" filename)
  (let ((pbuff (create-file-buffer filename))
	)
;;;    (save-excursion
      (set-buffer pbuff)
      (goto-char (point-min))
      (switch-to-buffer pbuff)
      ;; Write the heading...
      (sane-desc-pformat pbuff "
<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML//EN\">
<html> <head>
<title>SANE: %s</title>
</head>

<body bgcolor=%s>
<div align=center>
<img src=\"%s\" alt=\"SANE\">
<h1>%s</h1>
</div>
<hr>
<p>The following table summarizes the backends/drivers distributed with
SANE, and the hardware or software they support.  

<p>This is only a summary!
Please consult the manpages and the author-supplied webpages for more
detailed (and usually important) information concerning each backend.

<p>If you have new information or corrections, please send e-mail
to <a href=\"mailto:%s\">%s</a>.

<p>(For an explanation of the table, see the <a href=\"#legend\">legend</a>.)

<p>
"
;;;<base href=\"%s\">
;;;	       sane-desc-base-url
	       sane-desc-backend-html-title
	       sane-desc-backend-html-bgcolor
	       sane-desc-sane-logo
	       sane-desc-backend-html-title
	       sane-desc-email-address
	       sane-desc-email-address)
      ;; Write the table (w/ legend)...
      (sane-desc-generate-backend-table pbuff)
      ;; Write the footer...
      (sane-desc-pformat pbuff "
<hr>
<a href=\"%s\">[Back]</a>
<address>
<a href=\"mailto:%s\">%s</a> / SANE Development mailing list
</address>
<font size=-1>
This page was lasted updated on %s
</font>
</body> </html>
"
	       sane-desc-home-url
	       sane-desc-email-address
	       sane-desc-email-address
	       (current-time-string)
	       )
;;;      (set-visited-file-name sane-desc-backend-page-name)
;;;      )
      (write-file filename nil)
      (goto-char (point-min))
    nil
    ))


(defun sane-desc-generate-backend-table (buff)
  (sane-desc-pformat buff "
<div align=center>
<table border=1>
  <tr bgcolor=%s>
  <th align=center rowspan=2>Backend</th>
  <th align=center rowspan=2>Version</th>
  <th align=center rowspan=2>Status</th>
  <th align=center colspan=3>Supported Devices</th>
  <th align=center rowspan=2>Manual Page</th>
</tr>
<tr bgcolor=%s>
  <th align=center>Manufacturer</th>
  <th align=center>Model</th>
  <th align=center>Comment</th>
</tr>

"
	   sane-desc-table-head-bgcolor
	   sane-desc-table-head-bgcolor)
  (let ((bks (sane-desc-getval sane-desc-database :backends)))
    (while bks
      (message "Generating for %s" (sane-desc-getval (car bks) :name))
      (sane-desc-gen-backend-rows (car bks) buff)
      (setq bks (cdr bks))
      ))
  (princ "

</table>
</div>
" buff)
  ;; Print the Legend
 (sane-desc-pformat buff "
<font size=-1>
<h3><a name=\"legend\">Legend:</a></h3>
<blockquote>
<dl>
  <dt><b>Backend:</b></dt>
  <dd>Name of the backend, with a link to more extensive and detailed
  information, if it exists.</dd>

  <dt><b>Version:</b></dt>
  <dd>Version of backend/driver distributed in the lastest SANE release.
      Newer versions may be available from their home sites.</dd>

  <dt><b>Status:</b></dt>
  <dd>A vague indication of robustness and reliability.
      <ul>"
	  )
 (mapcar '(lambda (item)
	    (sane-desc-pformat buff "<li><font color=\"%s\">%s</font> %s\n"
		     (nth 1 (cdr item))
		     (nth 0 (cdr item))
		     (nth 2 (cdr item)))
	    nil)
	 sane-desc-status-codes)
 (sane-desc-pformat buff "
      </ul>
  </dd>

  <dt><b>Supported Devices:</b></dt>
  <dd>Which hardware the backend supports.</dd>

  <dt><b>Manual Page:</b></dt>
  <dd>A link to the man-page on-line, if it exists.</dd>
</dl>

</blockquote>
</font>
"
	  )
 )

;; two passes:  first to count various row-spans,
;;              second to actually write stuff.
(defun sane-desc-gen-backend-rows (bk buff)
  (let ((bkspan 0)
	(mfgspan nil))
    ; first, count the row-spans, per mfs and for the whole backend
    (setq mfgspan
	  (mapcar '(lambda (mfg)
		     (let ((span (length (sane-desc-getval mfg :devlist))))
		       (if (zerop span)
			   (setq span 1))
		       (setq bkspan (+ bkspan span))
		       (cons mfg span)
		       ))
		  (sane-desc-getval bk :mfglist)))
    (let ((mfglist (sane-desc-getval bk :mfglist)))
      ;; First row...
      (sane-desc-pformat buff "\n<tr>\n")
      ;; ...name of backend (+ url, if any)
      (sane-desc-pformat buff "  <td align=left rowspan=%d>" bkspan)
      (sane-desc-pformat-name-with-url buff (sane-desc-getval bk :name) (sane-desc-getval bk :url))
      (sane-desc-pformat buff "</td>\n")
      ;; ...version
      (sane-desc-pformat buff "  <td align=center rowspan=%d>%s</td>\n"
	       bkspan (or (sane-desc-getval bk :version) "?"))
      ;; ...status (in CoLoR)
      (let* ((status (sane-desc-getval bk :status))
	     (stuff (cdr (assq status sane-desc-status-codes))))
	(if (nth 1 stuff)
	    (sane-desc-pformat buff 
		     "  <td align=center rowspan=%d><font color=%s>%s</font></td>\n"
		     bkspan
		     (nth 1 stuff)
		     (nth 0 stuff))
	  (sane-desc-pformat buff "  <td align=center rowspan=%d>%s</td>\n"
		   bkspan (or (nth 0 stuff) "?"))
	    ))
      ;; ...*first* mfg, and *first* dev of that mfg
      (if mfglist
	  (sane-desc-pformat-mfg-and-first-dev buff (car mfglist) mfgspan)
	(sane-desc-pformat buff "   <td align=center colspan=3>?</td>\n"))
      
      ;; ...man-page
      (sane-desc-pformat buff "<td align=center rowspan=%d>" bkspan)
      (sane-desc-pformat-name-with-url buff
			     (sane-desc-getval bk :manpage)
			     (format sane-desc-manpage-url-format
				     (sane-desc-getval bk :manpage)))
      (sane-desc-pformat buff "</td>\n")
	
      ;; Now, go on and remaining rows...
      ;; ...do remaining devs of first mfg...
      (sane-desc-pformat-cdr-devs buff (sane-desc-getval (car mfglist) :devlist))
      ;; ...do remaining mfgs...
      (sane-desc-pformat-cdr-mfgs buff mfglist mfgspan)
      )
    ))


(defun sane-desc-pformat-mfg-and-first-dev (buff mfg mfgspan)
  ;; mfg name + device, or description
  (if (sane-desc-getval mfg :name)
      (progn
	;; ...name of mfg (+ url, if any)
	(sane-desc-pformat buff "<td align=center rowspan=%d>"
		 (cdr (assq mfg mfgspan)))
	(sane-desc-pformat-name-with-url buff (sane-desc-getval mfg :name) (sane-desc-getval mfg :url))
	(sane-desc-pformat buff "</td>\n")
	;; ...name of first dev
	(sane-desc-pformat-dev buff (car (sane-desc-getval mfg :devlist)))
	)
    (sane-desc-pformat-dev buff (car (sane-desc-getval mfg :devlist)) t)
    ))


(defun sane-desc-pformat-dev (buff dev &optional no-mfg)
  (if no-mfg
      (progn
	(sane-desc-pformat buff "<td align=center colspan=2>")
	(sane-desc-pformat-name-with-url buff (sane-desc-getval dev :desc) (sane-desc-getval dev :url)))
    (progn
      (sane-desc-pformat buff "<td>")
      (sane-desc-pformat-name-with-url buff (sane-desc-getval dev :desc) (sane-desc-getval dev :url)))
    )
  (sane-desc-pformat buff "</td>\n")
  (if (sane-desc-getval dev :comment)
      (sane-desc-pformat buff "<td>%s</td>\n" (sane-desc-getval dev :comment))
    (sane-desc-pformat buff "<td>&nbsp;</td>\n"))
  )


(defun sane-desc-pformat-cdr-devs (buff devlist)
  (setq devlist (cdr devlist))
  (while devlist
    (sane-desc-pformat buff "<tr>")
    (sane-desc-pformat-dev buff (car devlist))
    (sane-desc-pformat buff "</tr>\n")
    (setq devlist (cdr devlist)))
  )

(defun sane-desc-pformat-cdr-mfgs (buff mfglist mfgspan)
  (setq mfglist (cdr mfglist))
  (while mfglist
    (sane-desc-pformat buff "<tr>")
    (sane-desc-pformat-mfg-and-first-dev buff (car mfglist) mfgspan)
    (sane-desc-pformat buff "</tr>\n")
    (sane-desc-pformat-cdr-devs buff (sane-desc-getval (car mfglist) :devlist))
    (setq mfglist (cdr mfglist))
    ))

(defun sane-desc-pformat-name-with-url (buff name url)
  (if (and name url)
      (sane-desc-pformat buff "<a href=\"%s\">%s</a>" url name)
    (sane-desc-pformat buff "%s" (or name "?"))
    ))
  




(defun sane-desc-regenerate-backend-page ()
  (interactive)
  (call-interactively 'sane-desc-parse-directory)
  (call-interactively 'sane-desc-generate-backend-page)
  )


(defun sane-desc-doit ()
  (interactive)
  (sane-desc-parse-directory ".")
  (sane-desc-generate-backend-page "/home/httpd/html/sane/sane-backends.html"))
