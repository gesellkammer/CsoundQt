<CsoundSynthesizer>

; Id: F03_TR04.CSD mg (2006, rev.2009)
; author: marco gasperini (marcogsp at yahoo dot it)

; G.M. Koenig
; ESSAY (1957)

<CsOptions>
-W -f -oF03_TR04.wav
</CsOptions>

<CsInstruments>

sr     = 192000
kr     = 19200
ksmps  = 10
nchnls = 1

;=============================================
; 262.4 TRANSPOSITION (RESAMPLING)
;=============================================
	instr 1
itrasp	= -p4			; reads backwards
ifile	= p5	

iskip	= 18.296

a1	diskin2  ifile, itrasp, iskip

ifrq	= 96000*p4		; anti-aliasing filter
a1	tonex a1 , ifrq , 10
a1	tonex a1 , ifrq , 10

	out a1
	endin
;=============================================

</CsInstruments>
<CsScore>
t0	4572		; 76.2 cm/sec. tape speed (durations in cm)

;			p4	p5
;			itrasp	ifile
i1	0	1934.88	.5	"F02_REVcancer.wav"	; 25/50
; riverbero anteriore = 426.72*(1/.5)=853.44
e

</CsScore>
</CsoundSynthesizer>