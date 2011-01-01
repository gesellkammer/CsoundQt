<CsoundSynthesizer>

; Id: G01.CSD mg (2006, rev.2009)
; author: marco gasperini (marcogsp at yahoo dot it)

; G.M. Koenig
; ESSAY (1957)

<CsOptions>
-W -f -oG01.wav
</CsOptions>

<CsInstruments>

sr     = 192000
kr     = 19200
ksmps  = 10
nchnls = 1


;=============================================
; SINUS TONES (S)
;=============================================
	instr 1	
iamp	= ampdb(90+p4)
ifreq	= p5

a1	oscili iamp , ifreq , 1
aenv	expseg .001 , .005, 1 , p3-.01 ,1, .005,.001

aout	= a1 * aenv

	out aout
	endin
;=============================================

;=============================================
; FILTERED NOISE (N)
;=============================================
	instr 2
iamp	= ampdb(90+p4)
ifreq	= p5
ibw	= ifreq * .05		; filtered noise's bandwidth 5% of central frequency

a1	rnd31 iamp , 1 
k1	rms a1

afilt	butterbp a1 , ifreq , ibw
afilt	butterbp afilt , ifreq , ibw

aenv	expseg .001 , .005, .8 , p3-.01 ,.8 ,.005,.001

aout	gain afilt , k1
aout	= aout * aenv 

	out aout
	endin
;=============================================	

;=============================================
; FILTERED IMPULSES (I)
;=============================================
	instr 3
iamp	= ampdb(86+p4)
ifreq	= p5
ibw	= ifreq * .01		; filtered pulse's bandwidth 1% of central frequency

if1	= ifreq-(ibw/3)
if2	= ifreq+((2*ibw)/3)

				
a1	mpulse iamp , 0 

afilt	atonex a1 , if1 , 2
afilt	tonex afilt*800 , if2 , 2  
afilt	butterbp afilt*32 , ifreq , ibw*.5


aenv	linseg 1 , p3-.01, 1 , .01 , 0

aout	= afilt * aenv 

	out aout
	endin
;=============================================
</CsInstruments>
<CsScore>
;functions--------------------------------------------------
f1	0	8192	10	1	; sinusoid
;/functions--------------------------------------------------

t0	4572	; 76.2 cm/sec. tape speed (durations in cm)

;test--------------------------------------------------
;mute-------------------------------------------------
q 1 0 1
q 2 0 1
q 3 0 1
;/mute-------------------------------------------------
;/test-------------------------------------------------

;==================================================
; 170. MATERIAL G
; 171. total length: 1298.5 cm, 10 sections
;
; length     sequence 	
; 38.7    cm (4)
; 440.5   cm (10)
; 58      cm (5)
; 11.4    cm (1)
; 17.2    cm (2)
; 130.5   cm (7)
; 87      cm (6)
; 25.8    cm (3)
; 293.6   cm (9)
; 195.8   cm (8)
;==================================================

;==================================171.1
; 38.7 cm 9/8
;---------------------------------------
;			p4	p5
;			iamp	ifreq
;			[dB]	[Hz]
i1	0	5.6	0	3200	; S
i1	+	4.9	0	2691	; S
i1	+	3.5	0	2263	; S
i1	+	7	-.5	3064	; S
i1	+	6.3	-.2	2577	; S
i1	+	3.9	0	1903	; S
i1	+	3.1	0	2167	; S
i1	+	4.4	0	1600	; S
s                                   
t0	4572
;==================================171.2
; 440.5 cm 3/2
;---------------------------------------
i1	0	152.8	-6	2934	; S
i1	+	101.9	-6	1822    ; S
i1	+	30.2	-5	2467    ; S
i1	+	13.4	0	1345    ; S
i1	+	8.9	0	1532    ; S
i1	+	45.3	-1	2075    ; S
i1	+	20.1	-.7	1745    ; S
i3	372.6	67.9	-2	1131    ; I
s                                   
t0	4572
;==================================171.3
; 58 cm 8/7
;---------------------------------------
i1	0	9.6	-.3	1288	; S
i1	+	8.4	-1	2810    ; S
i1	+	5.7	0	1467    ; S
i1	+	4.3	0	2363    ; S
i1	+	11.1	-.5	1083    ; S
i1	+	6.5	0	1987    ; S
i1	+	5	0	951     ; S
i1	+	7.4	-.7	1233    ; S
s                                   
t0	4572
;==================================171.4
; 11.4 cm 12/11
;---------------------------------------
i1	0	1.3	0	1671	; S
i3	1.3	1.2	.5	911     ; I
i3	+	1.9	-.5	1037    ; I
i3	+	1.6	-3	1405    ; I
i1	6	1.5	0	800     ; S
i1	+	1	0	1181    ; S
i1	+	1.8	0	872     ; S
i1	+	1.1	0	766     ; S
s                                   
t0	4572
;==================================171.5
; 17.2 cm 11/10
;---------------------------------------
i1	0	1.7	0	993	; S
i1	+	1.5	0	734     ; S
i1	+	2.4	0	835     ; S
i3	5.6	2	3	673     ; I
i3	+	1.8	3.5	644     ; I
i3	+	2.7	3	702     ; I
i3	+	2.2	4	617     ; I
i3	+	2.9	4	591     ; I
s                                   
t0	4572
;==================================171.6
; 130.5 cm 6/5
;---------------------------------------
i1	0	7.9	0	566	; S
i1	+	28.3	0	542     ; S
i1	+	16.4	0	519     ; S
i1	+	11.4	0	497     ; S
i1	+	9.5	0	418     ; S
i3	73.5	19.7	7	436     ; I
i3	+	13.7	6	456     ; I
i3	+	23.6	8.5	351     ; I
s                                   
t0	4572
;==================================171.7
; 87 cm 7/6
;---------------------------------------
i3	0	11.1	6	476	; I
i3	+	9.5	8	367     ; I
i3	+	6	10 	295     ; I
i3	+	15	8	383     ; I
i1	41.6	12.9	0	248     ; S
i1	+	6.9	0	308     ; S
i1	+	17.5	0	400     ; S
i3	78.9	8.1	13	209     ; I
s                                   
t0	4572
;==================================171.8
; 25.8 cm 10/9
;---------------------------------------
i3	0	2.7	9.5	322	; I
i3	+	2.4	11	259     ; I
i3	+	4.1	14.5	176     ; I
i3	+	3.3	16	148     ; I
i3	+	3	9	336     ; I
i3	+	4.5	12.5	218     ; I
i3	+	3.7	11	271     ; I
i3	+	2.1	17.5	124     ; I
s                                   
t0	4572
;==================================171.9
; 293.6 cm 4/3
;---------------------------------------
i1	0	45.9	0	183	; S
i3	45.9	34.4	19	104     ; I
i3	+	14.5	10.5	283     ; I
i3	+	81.5	12.5	228     ; I
i3	+	61.2	15.5	154     ; I
i3	+	19.4	13.5	192     ; I
i3	+	10.9	17	130     ; I
i3	+	25.8	12	238     ; I
s                                   
t0	4572
;==================================171.10
; 195.8 cm 5/4
;---------------------------------------
i3	0	47.1	18.5	109	; I
i3	+	37.6	15.5	161     ; I
i3	+	19.3	13.5	200     ; I
i3	+	12.3	17	135     ; I
i3	+	9.9	15	168     ; I
i3	+	24.1	18.5	114     ; I
i3	+	15.4	16.5	141     ; I
i3	+	30.1	18	119     ; I
                                    
; total length: 1298.5 cm
e
</CsScore>
</CsoundSynthesizer>