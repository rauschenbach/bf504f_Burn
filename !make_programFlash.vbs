'-----------------------------------------------------
' Constants:
'-----------------------------------------------------
Const formatIntelHex = 0
Const formatBinary   = 1
Const formatASCII    = 2
Const eraseAffected  = 0
Const eraseAll       = 1
Const eraseNone      = 2
'-----------------------------------------------------
' Text format variables:
'-----------------------------------------------------
Dim NL
Dim TB
Dim TB2
Dim TB3

NL    = vbNewLine
TB    = vbTab
TB2   = TB+TB
TB3   = TB+TB+TB
usage = "cscript.exe --driver <driver file> [OPTIONS]"+NL+_
        "REQUIRED:"+NL+_
	NL+_
	"--driver <driver file>"+TB+_
	": specifies <driver file> for communication with the flash device"+NL+_
	NL+_
	"OPTIONS: "+NL+_
	"--image <image file>"+TB+_
	": specifies <image file> for loading into flash"+NL+_
	"--offsetL [addr]"+TB+_
	": address offset to use when executing image file load (default is 0x0)"+NL+_
	"--format"+TB2+_
	": format of the image file; 0 - Intel Hex (default), 1 - binary, 2 - ASCII"+NL+_
	"--verifyWrites"+TB2+_
	": verify write operations during the image file load (default is false)"+NL+_
	"--eraseOption"+TB2+_
	": option to clear the flash area where the image load is occurring;"+NL+_
	TB3+_
	"  0 - erase affected (default), 1 - erase all, 2 - erase none"+NL+_
	"--fill"+TB3+_
	": fill flash with values"+NL+_
	"--boot"+TB3+_
	": boot up the hardware board with the image in flash"+NL+_
	"--offsetF [addr]"+TB+_
	": address offset to use when executing fill (default is 0x0)"+NL+_
	"--count [n]"+TB2+_
	": number of values to write during fill operation (default is 16)"+NL+_
	"--stride [s]"+TB2+_
	": offset of the next write during fill operation (default is 1)"+NL+_
	"--value [val]"+TB2+_
	": value to be written into flash during fill (default is 0x0)"+NL+_
	"--eraseAll"+TB2+_
	": erase entire flash"+NL+_
	"--eraseBlocks [0,1,..]"+TB+_
	": erase specified sectors of flash (comma-separated list with no spaces);"+NL+_
	TB3+_
	"  if sector list is ommitted, the default is 0,1,2,3"+NL

'-----------------------------------------------------
' Helper variables:
'-----------------------------------------------------
Dim index

'-----------------------------------------------------
' Options:
'-----------------------------------------------------
Dim doLoadImage
Dim doVerifyWrites
Dim doEraseAll
Dim doEraseBlocks
Dim doLoadDriver
Dim doFill
Dim doBoot

'-----------------------------------------------------
' Values:
'-----------------------------------------------------
Dim fpDriver
Dim LdrImage
Dim LdrImage2
Dim LdrImageLoader
Dim blocks
Dim eraseOption
Dim imageFormat
Dim imageOffset
Dim imageOffset2
Dim loaderOffset
Dim fillOffset
Dim fillCount
Dim fillStride
Dim fillValue

'-----------------------------------------------------
' Initial values for script variables and options:
'-----------------------------------------------------
doLoadImage    = False
doVerifyWrites = False
doEraseAll     = False
doEraseBlocks  = False
doLoadDriver   = False
doVerifyWrites = False
doBoot         = False
blocks         = Array ( 0, 1, 2, 3 )
eraseOption    = eraseAffected
imageFormat    = formatIntelHex
imageOffset    = 0
fillOffset     = 0
fillCount      = 16
fillStride     = 1
fillValue      = 0

index = 0

'-----------------------------------------------------
' Check script usage:
'-----------------------------------------------------
If ( WScript.arguments.Count = 0 ) Then
	displayUsage
	WScript.Quit ( 0 )
End If

'-----------------------------------------------------
' Process user selected command line options:
'-----------------------------------------------------
For Each arg in WScript.arguments
	If     ( StrComp ( arg, "--driver" ) = 0 ) Then
		doLoadDriver    = True
		If ( ( index + 1 ) <= WScript.arguments.Count ) Then
			fpDriver = WScript.arguments ( index + 1 )
		End If
	ElseIf ( StrComp ( arg, "--image" ) = 0 ) Then
		doLoadImage     = True
		If ( ( index + 1 ) <= WScript.arguments.Count ) Then
			LdrImage = WScript.arguments ( index + 1 )
		End If
		If ( ( index + 2 ) <= WScript.arguments.Count ) Then
			LdrImage2 = WScript.arguments ( index + 2 )
		End If
	ElseIf ( StrComp ( arg, "--loaderimage" ) = 0 ) Then
		If ( ( index + 1 ) <= WScript.arguments.Count ) Then
			LdrImageLoader = WScript.arguments ( index + 1 )
		End If
	ElseIf ( StrComp ( arg, "--fill" ) = 0 ) Then
		doFill          = True
	ElseIf ( StrComp ( arg, "--boot" ) = 0 ) Then
		doBoot          = True
	ElseIf ( StrComp ( arg, "--verifyWrites" ) = 0 ) Then
		doVerifyWrites  = True
	ElseIf ( StrComp ( arg, "--eraseAll" ) = 0 ) Then
		doEraseAll      = True
	ElseIf ( StrComp ( arg, "--eraseBlocks" ) = 0 ) Then
		doEraseBlocks   = True
		If ( ( index + 1 ) <= WScript.arguments.Count ) Then
			blocks = Split ( WScript.arguments ( index + 1 ), "," )
		End If
	ElseIf ( StrComp ( arg, "--eraseOption" ) = 0 ) Then
		If ( ( index + 1 ) <= WScript.arguments.Count ) Then
			temp        = WScript.arguments ( index + 1 )
			temp        = Replace ( temp, "0x", "&h" )
			eraseOption = Eval ( temp )
		End If
	ElseIf ( StrComp ( arg, "--format" ) = 0 ) Then
		If ( ( index + 1 ) <= WScript.arguments.Count ) Then
			temp        = WScript.arguments ( index + 1 )
			temp        = Replace ( temp, "0x", "&h" )
			imageFormat = Eval ( temp )
		End If		
	ElseIf ( StrComp ( arg, "--offsetL" ) = 0 ) Then
		doImageOffset   = True
		If ( ( index + 1 ) <= WScript.arguments.Count ) Then
			temp        = WScript.arguments ( index + 1 )
			temp        = Replace ( temp, "0x", "&h" )
			imageOffset = Eval ( temp )
		End If
	ElseIf ( StrComp ( arg, "--offsetL2" ) = 0 ) Then
		doImageOffset   = True
		If ( ( index + 1 ) <= WScript.arguments.Count ) Then
			temp        = WScript.arguments ( index + 1 )
			temp        = Replace ( temp, "0x", "&h" )
			imageOffset2 = Eval ( temp )
		End If
	ElseIf ( StrComp ( arg, "--loaderoffset" ) = 0 ) Then
		If ( ( index + 1 ) <= WScript.arguments.Count ) Then
			temp        = WScript.arguments ( index + 1 )
			temp        = Replace ( temp, "0x", "&h" )
			loaderOffset = Eval ( temp )
		End If
	ElseIf ( StrComp ( arg, "--offsetF" ) = 0 ) Then
		If ( ( index + 1 ) <= WScript.arguments.Count ) Then
			temp       = WScript.arguments ( index + 1 )
			temp       = Replace ( temp, "0x", "&h" )
			fillOffset = Eval ( temp )
		End If
	ElseIf ( StrComp ( arg, "--count" ) = 0 ) Then
		If ( ( index + 1 ) <= WScript.arguments.Count ) Then
			temp = WScript.arguments ( index + 1 )	
			temp = Replace ( temp, "0x", "&h" )	
			fillCount = Eval ( temp )
		End If
	ElseIf ( StrComp ( arg, "--stride" ) = 0 ) Then
		If ( ( index + 1 ) <= WScript.arguments.Count ) Then
			temp = WScript.arguments ( index + 1 )
			temp = Replace ( temp, "0x", "&h" )
			fillStride = Eval ( temp )
		End If
	ElseIf ( StrComp ( arg, "--value" ) = 0 ) Then
		If ( ( index + 1 ) <= WScript.arguments.Count ) Then
			temp = WScript.arguments ( index + 1 )
			temp = Replace ( temp, "0x", "&h" )
			fillValue = Eval ( temp )
		End If
	End If
	index = index + 1
Next

'-----------------------------------------------------
' Print user selected options (for debugging the
' script itself):
'-----------------------------------------------------
Sub checkArgs ( )
	WScript.StdOut.WriteLine "FPDriver: "       + FPDriver
	WScript.StdOut.WriteLine "LdrImage: "       + LdrImage
	WScript.StdOut.WriteLine "doEraseBlocks: "  + CStr ( doEraseBlocks )
	WScript.StdOut.WriteLine "doEraseAll: "     + CStr ( doEraseAll )
	WScript.StdOut.WriteLine "doLoadImage: "    + CStr ( doLoadImage )
	WScript.StdOut.WriteLine "doVerifyWrites: " + CStr ( doVerifyWrites )
	WScript.StdOut.WriteLine "doBoot: "         + CStr ( doBoot )
	WScript.StdOut.WriteLine "eraseOption: "    + CStr ( eraseOption )
	WScript.StdOut.WriteLine "imageFormat: "    + CStr ( imageFormat )	
	WScript.StdOut.WriteLine "imageOffset: "    + CStr ( imageOffset )
	WScript.StdOut.WriteLine "fillOffset: "     + CStr ( fillOffset )
	WScript.StdOut.WriteLine "fillCount: "      + CStr ( fillCount )
	WScript.StdOut.WriteLine "fillStride: "     + CStr ( fillStride )
	WScript.StdOut.WriteLine "fillValue: "      + "0x" + Hex ( fillValue )	
	WScript.StdOut.Write "eraseBlocks: "
	For i = 0 to UBound ( blocks )
		WScript.StdOut.Write CStr ( blocks(i) ) + " "
	Next
	WScript.StdOut.WriteLine ""
End Sub

'-----------------------------------------------------
' Print out script usage:
'-----------------------------------------------------
Sub displayUsage ( )
	WScript.StdOut.Write usage
End Sub

'-----------------------------------------------------
' Connect to the Idde object via COM and initialize
' some settings:
'-----------------------------------------------------
Set app = CreateObject( "VisualDSP.ADspApplication" )
app.Interactive = True
app.Visible     = True

    Set session = app.CreateSession( "flashingsession", "Blackfin Emulators/EZ-KIT Lites", "ADSP-BF504F via ICE-100B", "ADSP-BF504F" )

    If (session Is Nothing) Then
        app.PrintText tabConsole, "CreateSession failed"
	WScript.Quit ( 0 )
    End If

'-----------------------------------------------------
' Retrieve a reference to the Flash Programmer plug-in
' object:
'-----------------------------------------------------
WScript.StdOut.WriteLine "Starting the FlashProgrammer plugin..."
Set FPPlugin        = app.PluginList.Item("Flash Programmer")
Set FlashProgrammer = FPPlugin.Open()

'-----------------------------------------------------
' Load the flash driver:
'-----------------------------------------------------
WScript.StdOut.WriteLine "Loading the FP driver..."
FlashProgrammer.LoadDriver FPDriver

'-----------------------------------------------------
' First, perform all user selected erase options:
'-----------------------------------------------------
If ( doEraseAll = True ) Then
	WScript.StdOut.WriteLine "Erasing all flash..."
	FlashProgrammer.EraseAll()
ElseIf ( doEraseBlocks = True ) Then
	WScript.StdOut.WriteLine "Erasing Flash blocks..."
	For i = 0 To UBound ( blocks )
		strBlock = blocks(i)
		WScript.StdOut.WriteLine "Erasing block " + strBlock + "..."
		FlashProgrammer.EraseBlock CInt ( strBlock )
	Next
End If

'-----------------------------------------------------
' Do any user chosen flash fills:
'-----------------------------------------------------
If ( doFill = True ) Then
	If ( fillCount < 0 ) Then
		fillCount = 16
	End If
	
	WScript.StdOut.WriteLine "Filling flash: count " + CStr ( fillCount ) + "..."
	FlashProgrammer.FillFlash fillOffset, fillCount, fillStride, fillValue
End If

'-----------------------------------------------------
' Load the user specified image file:
'-----------------------------------------------------
If ( doLoadImage = True ) Then
	WScript.StdOut.WriteLine "Programming Flash with the image File..."
	FlashProgrammer.LoadFile LdrImage, imageFormat, doVerify, eraseOption, imageOffset
	FlashProgrammer.LoadFile LdrImage2, imageFormat, doVerify, eraseOption, imageOffset2
	if (LdrImageLoader <> "") Then
		WScript.StdOut.WriteLine "Writing bootloader"
		FlashProgrammer.LoadFile LdrImageLoader, imageFormat, doVerify, eraseOption, loaderOffset
	End If
End If

'-----------------------------------------------------
' Boot up the target board if user has selected this
' option:
'-----------------------------------------------------
If ( doBoot = True ) Then
	WScript.StdOut.WriteLine "Booting up the board from flash..."
	While ( app.ActiveSession.ActiveProcessor.BreakpointList.Count > 0 ):
		app.ActiveSession.ActiveProcessor.BreakpointList.RemoveBreakpointByIndex( 0 )
	WEnd	
	session.ProcessorList(0).Reset(True)
	session.ProcessorList(0).Run(False)
End If

WScript.StdOut.WriteLine "All done!"

	app.DisconnectSession

