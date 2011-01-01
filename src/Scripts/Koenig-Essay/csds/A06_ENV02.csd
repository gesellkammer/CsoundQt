<CsoundSynthesizer>

; Id: A06_ENV02.CSD mg (2006, rev.2009)
; author: marco gasperini (marcogsp at yahoo dot it)

; G.M. Koenig
; ESSAY (1957)

<CsOptions>
-W -f -oA06_ENV02.wav
</CsOptions>

<CsInstruments>

sr     = 192000
kr     = 19200
ksmps  = 10
nchnls = 1

;=============================================
; 215.2 INTENSITY CURVES
;=============================================
	instr 1
iamp1	= ampdb(90+p4)/ 31631
iamp2	= ampdb(90+p5)/ 31631
iamp3	= ampdb(90+p6)/ 31631

ifile 	= p7

a1	diskin2 ifile, 1

aenv	expseg iamp1,.7,iamp1,(p3-.7)/2,iamp2,(p3-.7)/2,iamp3

aout	=  a1*aenv 
	out aout
	endin
;=============================================

</CsInstruments>
<CsScore>
t0	4572		; 76.2 cm/sec. tape speed (durations in cm)

;			p4	p5	p6	p7
;			iamp1	iamp2	iamp3	ifile
;			[dB]	[dB]	[dB]
i1	0	384.7	0	-25	2	"A04_FILT02.wav"

e

</CsScore>
</CsoundSynthesizer>