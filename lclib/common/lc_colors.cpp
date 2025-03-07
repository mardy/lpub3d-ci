#include "lc_global.h"
#include "lc_colors.h"
#include "lc_file.h"
#include <float.h>

std::vector<lcColor> gColorList;
lcColorGroup gColorGroups[LC_NUM_COLORGROUPS];
int gNumUserColors;
int gEdgeColor;
int gDefaultColor;

lcVector4 gInterfaceColors[LC_NUM_INTERFACECOLORS] = // todo: make the colors configurable and include the grid and other hardcoded colors here as well.
{
	lcVector4(0.898f, 0.298f, 0.400f, 1.000f), // LC_COLOR_SELECTED
	lcVector4(0.400f, 0.298f, 0.898f, 1.000f), // LC_COLOR_FOCUSED
	lcVector4(0.800f, 0.800f, 0.800f, 1.000f), // LC_COLOR_DISABLED
	lcVector4(0.500f, 0.800f, 0.500f, 1.000f), // LC_COLOR_CAMERA
	lcVector4(0.500f, 0.800f, 0.500f, 1.000f), // LC_COLOR_LIGHT
	lcVector4(0.500f, 0.800f, 0.500f, 0.500f), // LC_COLOR_CONTROL_POINT
	lcVector4(0.400f, 0.298f, 0.898f, 0.500f), // LC_COLOR_CONTROL_POINT_FOCUSED
	lcVector4(0.098f, 0.898f, 0.500f, 1.000f)  // LC_COLOR_HIGHLIGHT
};

static void GetToken(char*& Ptr, char* Token)
{
	while (*Ptr && *Ptr <= 32)
		Ptr++;

	while (*Ptr > 32)
		*Token++ = *Ptr++;

	*Token = 0;
}

int lcGetBrickLinkColor(int ColorIndex)
{
	struct lcBrickLinkEntry
	{
		int LDraw;
		int BrickLink;
	};

	lcBrickLinkEntry BrickLinkColors[] =
	{
		{   0,  11 }, // Black
		{   1,   7 }, // Blue
		{   2,   6 }, // Green
		{   3,  39 }, // Dark Turquoise
		{   4,   5 }, // Red
		{   5,  47 }, // Dark Pink
		{   6,   8 }, // Brown
		{   7,   9 }, // Light Gray
		{   8,  10 }, // Dark Gray
		{   9,  62 }, // Light Blue
		{  10,  36 }, // Bright Green
		{  11,  40 }, // Light Turquoise
		{  12,  25 }, // Salmon
		{  13,  23 }, // Pink
		{  14,   3 }, // Yellow
		{  15,   1 }, // White
		{  17,  38 }, // Light Green
		{  18,  33 }, // Light Yellow
		{  19,   2 }, // Tan
		{  20,  44 }, // Light Violet
		{  21,  46 }, // Glow In Dark Opaque
		{  22,  24 }, // Purple
		{  23, 109 }, // Dark Blue-Violet
		{  25,   4 }, // Orange
		{  26,  71 }, // Magenta
		{  27,  34 }, // Lime
		{  28,  69 }, // Dark Tan
		{  29, 104 }, // Bright Pink
		{  33,  14 }, // Trans-Dark Blue
		{  34,  20 }, // Trans-Green
		{  35, 108 }, // Trans-Bright Green
		{  36,  17 }, // Trans-Red
		{  37,  51 }, // Trans-Purple
		{  40,  13 }, // Trans-Black
		{  41,  15 }, // Trans-Light Blue
		{  42,  16 }, // Trans-Neon Green
		{  43, 113 }, // Trans-Very Lt Blue
		{  45,  50 }, // Trans-Dark Pink
		{  46,  19 }, // Trans-Yellow
		{  47,  12 }, // Trans-Clear
		{  54, 121 }, // Trans-Neon Yellow
		{  57,  18 }, // Trans-Neon Orange
		{  60,  57 }, // Chrome Antique Brass
		{  61,  52 }, // Chrome Blue
		{  62,  64 }, // Chrome Green
		{  63,  82 }, // Chrome Pink
		{  64, 122 }, // Chrome Black
		{  68,  96 }, // Very Light Orange
		{  69,  93 }, // Light Purple
		{  70,  88 }, // Reddish Brown
		{  71,  86 }, // Light Bluish Gray
		{  72,  85 }, // Dark Bluish Gray
		{  73,  42 }, // Medium Blue
		{  74,  37 }, // Medium Green
		{  75, 116 }, // Speckle Black-Copper
		{  76, 117 }, // Speckle DBGray-Silver
		{  77,  56 }, // Light Pink
		{  78,  90 }, // Light Flesh
		{  79,  60 }, // Milky White
		{  84, 150 }, // Medium Dark Flesh
		{  85,  89 }, // Dark Purple
		{  86,  91 }, // Dark Flesh
		{  92,  28 }, // Flesh
		{ 100,  26 }, // Light Salmon
		{ 110,  43 }, // Violet
		{ 112,  97 }, // Blue-Violet
		{ 114, 100 }, // Glitter Trans-Dark Pink
		{ 115,  76 }, // Medium Lime
		{ 117, 101 }, // Glitter Trans-Clear
		{ 118,  41 }, // Aqua
		{ 120,  35 }, // Light Lime
		{ 125,  32 }, // Light Orange
		{ 129, 102 }, // Glitter Trans-Purple
		{ 132, 111 }, // Speckle Black-Silver
		{ 134,  84 }, // Copper
		{ 135,  66 }, // Pearl Light Gray
		{ 137,  78 }, // Metal Blue
		{ 142,  61 }, // Pearl Light Gold
		{ 143,  74 }, // Trans-Medium Blue
		{ 148,  77 }, // Pearl Dark Gray
		{ 150, 119 }, // Pearl Very Light Gray
		{ 151,  99 }, // Very Light Bluish Gray
		{ 178,  81 }, // Flat Dark Gold
		{ 179,  95 }, // Flat Silver
		{ 182,  98 }, // Trans-Orange
		{ 183,  83 }, // Pearl White
		{ 191, 110 }, // Bright Light Orange
		{ 212, 105 }, // Bright Light Blue
		{ 216,  27 }, // Rust
		{ 226, 103 }, // Bright Light Yellow
		{ 230, 107 }, // Trans-Pink
		{ 232,  87 }, // Sky Blue
		{ 236, 114 }, // Trans-Light Purple
		{ 272,  63 }, // Dark Blue
		{ 288,  80 }, // Dark Green
		{ 294, 118 }, // Glow In Dark Trans
		{ 297, 115 }, // Pearl Gold
		{ 308, 120 }, // Dark Brown
		{ 313,  72 }, // Maersk Blue
		{ 320,  59 }, // Dark Red
		{ 321, 153 }, // Dark Azure
		{ 323, 152 }, // Light Aqua
		{ 334,  21 }, // Chrome Gold
		{ 335,  58 }, // Sand Red
		{ 351,  94 }, // Medium Dark Pink
		{ 366,  29 }, // Earth Orange
		{ 373,  54 }, // Sand Purple
		{ 378,  48 }, // Sand Green
		{ 379,  55 }, // Sand Blue
		{ 383,  22 }, // Chrome Silver
		{ 450, 106 }, // Fabuland Brown
		{ 462,  31 }, // Medium Orange
		{ 484,  68 }, // Dark Orange
		{ 503,  49 }, // Very Light Gray
		{ 65,    3 }, // Rubber Yellow
		{ 66,   19 }, // Rubber Trans Yellow
		{ 67,   12 }, // Rubber Trans Clear
		{ 256,  11 }, // Rubber Black
		{ 273,   7 }, // Rubber Blue
		{ 324,   5 }, // Rubber Red
		{ 350,   4 }, // Rubber Orange
		{ 375,   9 }, // Rubber Light Gray
		{ 406,  63 }, // Rubber Dark Blue
		{ 449,  24 }, // Rubber Purple
		{ 490,  34 }, // Rubber Lime
		{ 496,  86 }, // Rubber Light Bluish Gray
		{ 504,  95 }, // Rubber Flat Silver
		{ 511,   1 }, // Rubber White
	};

	int ColorCode = gColorList[ColorIndex].Code;

	for (unsigned int Color = 0; Color < sizeof(BrickLinkColors) / sizeof(BrickLinkColors[0]); Color++)
		if (BrickLinkColors[Color].LDraw == ColorCode)
			return BrickLinkColors[Color].BrickLink;

	return 0;
}

bool lcLoadColorFile(lcFile& File)
{
	char Line[1024], Token[1024];
	std::vector<lcColor>& Colors = gColorList;
	lcColor Color, MainColor, EdgeColor;

	Colors.clear();

	for (int GroupIdx = 0; GroupIdx < LC_NUM_COLORGROUPS; GroupIdx++)
		gColorGroups[GroupIdx].Colors.clear();

	gColorGroups[0].Name = QApplication::tr("Solid Colors", "Colors");
	gColorGroups[1].Name = QApplication::tr("Translucent Colors", "Colors");
	gColorGroups[2].Name = QApplication::tr("Special Colors", "Colors");
/*** LPub3D Mod - load color entry - add LPub3D color group ***/
	gColorGroups[3].Name = QApplication::tr("LPub3D Colors", "Colors");
/*** LPub3D Mod end ***/

	MainColor.Code = 16;
	MainColor.Translucent = false;
	MainColor.Value[0] = 1.0f;
	MainColor.Value[1] = 1.0f;
	MainColor.Value[2] = 0.5f;
	MainColor.Value[3] = 1.0f;
	MainColor.Edge[0] = 0.2f;
	MainColor.Edge[1] = 0.2f;
	MainColor.Edge[2] = 0.2f;
	MainColor.Edge[3] = 1.0f;
	strcpy(MainColor.Name, "Main Color");
	strcpy(MainColor.SafeName, "Main_Color");

	EdgeColor.Code = 24;
	EdgeColor.Translucent = false;
	EdgeColor.Value[0] = 0.5f;
	EdgeColor.Value[1] = 0.5f;
	EdgeColor.Value[2] = 0.5f;
	EdgeColor.Value[3] = 1.0f;
	EdgeColor.Edge[0] = 0.2f;
	EdgeColor.Edge[1] = 0.2f;
	EdgeColor.Edge[2] = 0.2f;
	EdgeColor.Edge[3] = 1.0f;
	strcpy(EdgeColor.Name, "Edge Color");
	strcpy(EdgeColor.SafeName, "Edge_Color");

	while (File.ReadLine(Line, sizeof(Line)))
	{
		char* Ptr = Line;

		GetToken(Ptr, Token);
		if (strcmp(Token, "0"))
			continue;

		GetToken(Ptr, Token);
		strupr(Token);
		if (strcmp(Token, "!COLOUR"))
			continue;

		bool GroupTranslucent = false;
		bool GroupSpecial = false;

		Color.Code = ~0U;
		Color.Translucent = false;
		Color.Value[0] = FLT_MAX;
		Color.Value[1] = FLT_MAX;
		Color.Value[2] = FLT_MAX;
		Color.Value[3] = 1.0f;
		Color.Edge[0] = FLT_MAX;
		Color.Edge[1] = FLT_MAX;
		Color.Edge[2] = FLT_MAX;
		Color.Edge[3] = 1.0f;
/*** LPub3D Mod - use 3DViewer colors ***/
		Color.CValue = ~0U;
		Color.EValue = ~0U;
		Color.Alpha = 255;
/*** LPub3D Mod end ***/

		GetToken(Ptr, Token);
		strncpy(Color.Name, Token, sizeof(Color.Name));
		Color.Name[LC_MAX_COLOR_NAME - 1] = 0;
		strncpy(Color.SafeName, Color.Name, sizeof(Color.SafeName));

		for (char* Underscore = strchr((char*)Color.Name, '_'); Underscore; Underscore = strchr(Underscore, '_'))
			*Underscore = ' ';

		for (GetToken(Ptr, Token); Token[0]; GetToken(Ptr, Token))
		{
			strupr(Token);

			if (!strcmp(Token, "CODE"))
			{
				GetToken(Ptr, Token);
				Color.Code = atoi(Token);
			}
			else if (!strcmp(Token, "VALUE"))
			{
				GetToken(Ptr, Token);
				if (Token[0] == '#')
					Token[0] = ' ';

				int Value;
				if (sscanf(Token, "%x", &Value) != 1)
					Value = 0;
/*** LPub3D Mod - use 3DViewer colors ***/
				Color.CValue = Value;
/*** LPub3D Mod end ***/
				Color.Value[2] = (float)(Value & 0xff) / 255.0f;
				Value >>= 8;
				Color.Value[1] = (float)(Value & 0xff) / 255.0f;
				Value >>= 8;
				Color.Value[0] = (float)(Value & 0xff) / 255.0f;
			}
			else if (!strcmp(Token, "EDGE"))
			{
				GetToken(Ptr, Token);
				if (Token[0] == '#')
					Token[0] = ' ';

				int Value;
				if (sscanf(Token, "%x", &Value) != 1)
					Value = 0;
/*** LPub3D Mod - use 3DViewer colors ***/
				Color.EValue = Value;
/*** LPub3D Mod end ***/

				Color.Edge[2] = (float)(Value & 0xff) / 255.0f;
				Value >>= 8;
				Color.Edge[1] = (float)(Value & 0xff) / 255.0f;
				Value >>= 8;
				Color.Edge[0] = (float)(Value & 0xff) / 255.0f;
			}
			else if (!strcmp(Token, "ALPHA"))
			{
				GetToken(Ptr, Token);
				int Value = atoi(Token);
				Color.Value[3] = (float)(Value & 0xff) / 255.0f;
				if (Value != 255)
					Color.Translucent = true;
/*** LPub3D Mod - use 3DViewer colors ***/
				Color.Alpha = Value;
/*** LPub3D Mod end ***/
				if (Value == 128)
					GroupTranslucent = true;
				else if (Value != 0)
					GroupSpecial = true;
			}
			else if (!strcmp(Token, "CHROME") || !strcmp(Token, "PEARLESCENT") || !strcmp(Token, "RUBBER") ||
			         !strcmp(Token, "MATTE_METALIC") || !strcmp(Token, "METAL") || !strcmp(Token, "LUMINANCE"))
			{
				GroupSpecial = true;
			}
			else if (!strcmp(Token, "MATERIAL"))
			{
				GroupSpecial = true;
				break; // Material is always last so ignore it and the rest of the line.
			}
		}

		if (Color.Code == ~0U || Color.Value[0] == FLT_MAX)
			continue;

		if (Color.Edge[0] == FLT_MAX)
		{
			Color.Edge[0] = 33.0f / 255.0f;
			Color.Edge[1] = 33.0f / 255.0f;
			Color.Edge[2] = 33.0f / 255.0f;
		}

		bool Duplicate = false;

		for (lcColor& ExistingColor : Colors)
		{
			if (ExistingColor.Code == Color.Code)
			{
				ExistingColor = Color;
				Duplicate = true;
				break;
			}
		}

		if (Duplicate)
			continue;

		if (Color.Code == 16)
		{
			MainColor = Color;
			continue;
		}

		if (Color.Code == 24)
		{
			EdgeColor = Color;
			continue;
		}

		Colors.push_back(Color);

		if (GroupSpecial)
			gColorGroups[LC_COLORGROUP_SPECIAL].Colors.push_back((int)Colors.size() - 1);
		else if (GroupTranslucent)
			gColorGroups[LC_COLORGROUP_TRANSLUCENT].Colors.push_back((int)Colors.size() - 1);
		else
			gColorGroups[LC_COLORGROUP_SOLID].Colors.push_back((int)Colors.size() - 1);
	}

	gDefaultColor = (int)Colors.size();
	Colors.push_back(MainColor);
	gColorGroups[LC_COLORGROUP_SOLID].Colors.push_back(gDefaultColor);

	gNumUserColors = (int)Colors.size();

	gEdgeColor = (int)Colors.size();
	Colors.push_back(EdgeColor);

	return Colors.size() > 2;
}

/*** LPub3D Mod - load color entry ***/
bool lcLoadColorEntry(const char* ColorEntry)
{
//        qDebug() << qPrintable(QString("DEBUG Load color entry %1.")
//                               .arg(ColorEntry));
	char Line[1024], Token[1024];
	std::vector<lcColor>& Colors = gColorList;
	lcColor Color;

	int gNumColorBeforeAdd = (int)Colors.size();

	strncpy(Line, ColorEntry, sizeof(Line));

	char* Ptr = Line;

	GetToken(Ptr, Token);
	if (strcmp(Token, "0"))   // if no match, token evaluates to 1 (true)
		return false;

	GetToken(Ptr, Token);
	strupr(Token);
	if (strcmp(Token, "!COLOUR"))
		return false;

	Color.Code = ~0U;
	Color.Translucent = false;
	Color.Value[0] = FLT_MAX;
	Color.Value[1] = FLT_MAX;
	Color.Value[2] = FLT_MAX;
	Color.Value[3] = 1.0f;
	Color.Edge[0] = FLT_MAX;
	Color.Edge[1] = FLT_MAX;
	Color.Edge[2] = FLT_MAX;
	Color.Edge[3] = 1.0f;
	Color.CValue = ~0U;
	Color.EValue = ~0U;
	Color.Alpha = 255;

	GetToken(Ptr, Token);
	strncpy(Color.Name, Token, sizeof(Color.Name));
	Color.Name[LC_MAX_COLOR_NAME - 1] = 0;
	strncpy(Color.SafeName, Color.Name, sizeof(Color.SafeName));

	for (char* Underscore = strchr((char*)Color.Name, '_'); Underscore; Underscore = strchr(Underscore, '_'))
		*Underscore = ' ';

	for (GetToken(Ptr, Token); Token[0]; GetToken(Ptr, Token))
	{
		strupr(Token);

		if (!strcmp(Token, "CODE"))
		{
			GetToken(Ptr, Token);
			Color.Code = atoi(Token);
		}
		else if (!strcmp(Token, "VALUE"))
		{
			GetToken(Ptr, Token);
			if (Token[0] == '#')
				Token[0] = ' ';

			int Value;
			if (sscanf(Token, "%x", &Value) != 1)
				Value = 0;
			Color.EValue = Value;

			Color.Value[2] = (float)(Value & 0xff) / 255.0f;
			Value >>= 8;
			Color.Value[1] = (float)(Value & 0xff) / 255.0f;
			Value >>= 8;
			Color.Value[0] = (float)(Value & 0xff) / 255.0f;
		}
		else if (!strcmp(Token, "EDGE"))
		{
			GetToken(Ptr, Token);
			if (Token[0] == '#')
				Token[0] = ' ';

			int Value;
			if (sscanf(Token, "%x", &Value) != 1)
				Value = 0;
			Color.EValue = Value;

			Color.Edge[2] = (float)(Value & 0xff) / 255.0f;
			Value >>= 8;
			Color.Edge[1] = (float)(Value & 0xff) / 255.0f;
			Value >>= 8;
			Color.Edge[0] = (float)(Value & 0xff) / 255.0f;
		}
		else if (!strcmp(Token, "ALPHA"))
		{
			GetToken(Ptr, Token);
			int Value = atoi(Token);
			Color.Alpha = Value;
			Color.Value[3] = (float)(Value & 0xff) / 255.0f;
			if (Value != 255)
			{
				Color.Translucent = true;
				Color.Edge[3] = (float)(Value & 0xff) / 255.0f;
			}
		}
	}

	if (Color.Code == ~0U || Color.Value[0] == FLT_MAX)  // Code or Value attribute not set
		return false;

	if (Color.Edge[0] == FLT_MAX)
	{
		Color.Edge[0] = 33.0f / 255.0f;
		Color.Edge[1] = 33.0f / 255.0f;
		Color.Edge[2] = 33.0f / 255.0f;
	}

	bool Duplicate = false;

	for (lcColor& ExistingColor : Colors)
	{
		if (ExistingColor.Code == Color.Code)
		{
			ExistingColor = Color;
			Duplicate = true;
			break;
		}
	}

	if (Duplicate)
		return true;

	Colors.push_back(Color);

	gColorGroups[LC_COLORGROUP_LPUB3D].Colors.push_back((int)Colors.size() - 1);

    gNumUserColors = int(Colors.size());

//        qDebug() << qPrintable(QString("DEBUG Colours New Size %1, Old Size %2.")
//                               .arg(Colors.GetSize()).arg(gNumColorBeforeAdd));

    return int(Colors.size()) > gNumColorBeforeAdd;
}
/*** LPub3D Mod end ***/

void lcLoadDefaultColors()
{
	QResource Resource(":/resources/ldconfig.ldr");

	if (!Resource.isValid())
		return;

	QByteArray Data;

	if (Resource.isCompressed())
		Data = qUncompress(Resource.data(), Resource.size());
	else
		Data = QByteArray::fromRawData((const char*)Resource.data(), Resource.size());

	lcMemFile MemSettings;

	MemSettings.WriteBuffer(Data.constData(), Data.size());
	MemSettings.Seek(0, SEEK_SET);
	lcLoadColorFile(MemSettings);
}

int lcGetColorIndex(quint32 ColorCode)
{
	for (size_t ColorIdx = 0; ColorIdx < gColorList.size(); ColorIdx++)
		if (gColorList[ColorIdx].Code == ColorCode)
			return (int)ColorIdx;

	lcColor Color;

	Color.Code = ColorCode;
	Color.Translucent = false;
	Color.Edge[0] = 0.2f;
	Color.Edge[1] = 0.2f;
	Color.Edge[2] = 0.2f;
	Color.Edge[3] = 1.0f;

	if (ColorCode & LC_COLOR_DIRECT)
	{
		Color.Value[0] = (float)((ColorCode & 0xff0000) >> 16) / 255.0f;
		Color.Value[1] = (float)((ColorCode & 0x00ff00) >>  8) / 255.0f;
		Color.Value[2] = (float)((ColorCode & 0x0000ff) >>  0) / 255.0f;
		Color.Value[3] = 1.0f;
		sprintf(Color.Name, "Color %06X", ColorCode & 0xffffff);
		sprintf(Color.SafeName, "Color_%06X", ColorCode & 0xffffff);
	}
	else
	{
		Color.Value[0] = 0.5f;
		Color.Value[1] = 0.5f;
		Color.Value[2] = 0.5f;
		Color.Value[3] = 1.0f;
		sprintf(Color.Name, "Color %03d", ColorCode);
		sprintf(Color.SafeName, "Color_%03d", ColorCode);
	}

	gColorList.push_back(Color);
	return (int)gColorList.size() - 1;
}
