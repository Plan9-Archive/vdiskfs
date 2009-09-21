The new Usenix LaTeX-2e style files, completely rewritten from scratch.
Major new features include

- conform to latest USENIX publishing standards

- include four modes of publishing, to help take the paper though its stages
  from draft to camera-ready.

- designed to squeeze unnecessary white spaces that latex uses, so as to
  make the paper more readable and allow for more text to fit in few pages
  (while not making it appear too dense).

- includes annotation features to allow authors and readers to exchange
  information, to use as reminders, and more.

##############################################################################
Contents of this directory:

README.txt: this file

ChangeLog: changes that were made in previous revisions, so you can track
	what we're doing.

usetex-v1.cls: latex2e class file for new Usenix papers, version 1.  Read
	the comments at the top of this file to understand how to use it.
	Also see the template file.

template-v1.tex: sample file showing off features of usetex-v1.cls.  This is
	an original template from which we've instantiated several other
	ps/pdf samples showing the template in the four different modes of
	operation.

template-v1-finalversion.pdf, template-v1-finalversion.ps: the sample file
	in final-version format or camera-ready copy (CRC) style.

template-v1-proof.pdf, template-v1-proof.ps: the sample file in a format
	suitable for proof-reading by shepherds and others.

template-v1-webversion.pdf, template-v1-webversion.ps: the sample in a
	format suitable for for placing on a Web page.

template-v1-workingdraft.pdf, template-v1-workingdraft.ps: the sample in a
	draft format suitable when making lots of changes to the paper.

url.sty: useful file for line-breaking URLs in latex files (not changed from
	the one distributed by teTeX).  Most recent latex distros have it
	already.

autotools-paper-published.ps: Erez Zadok's FREENIX 2002 "autotools" paper,
	as it was published.

autotools-using-usetex.ps: Zadok's same paper, formatted using
	usetex-v1.cls, to show how the final version might have looked
	differently had it used usetex-v1.cls.

sample.fig, sample.eps: dummy figure done in XFig for the sample file.
##############################################################################


If you have any questions or concerns about these latex templates, please
contact one of the following people:

1. Bart Massey <bart@cs.pdx.edu>
2. Chuck Cranor <chuck@research.att.com>
3. Erez Zadok <ezk@cs.sunysb.edu>

Note: Bart is the primary author of these new latex templates and has
graciously agreed to maintain and enhance them based on your feedback.
These will hopefully eventually become the latest standard templates for
Usenix authors to use.

Cheers,
Erez.
