#include "stbtraits.h"

static const stb_feature_t features_vusolo2[] =
{
	{
		.type			= stb_traits_type_string_enum,
		.description	= "Video codec",
		.id				= "vcodec",
		.settable		= false,
		.api_data		= 0,
		.value			=
		{
			.string_enum_type =
			{
				.default_value	= "h264",
				.enum_values	= 
				{
					"h264", 0,
				},
			},
		},
	},
	{
		.type			= stb_traits_type_string_enum,
		.description	= "Audio codec",
		.id				= "acodec",
		.settable		= false,
		.api_data		= 0,
		.value			=
		{
			.string_enum_type =
			{
				.default_value	= "aac",
				.enum_values	= 
				{
					"aac", 0,
				},
			},
		},
	},
	{
		.type			= stb_traits_type_int,
		.description	= "Total stream bit rate",
		.id				= "bitrate",
		.settable		= true,
		.api_data		= "bitrate",
		.value			=
		{
			.int_type =
			{
				.default_value	= 400,
				.min_value		= 50,
				.max_value		= 1000,
				.scaling_factor	= 1000,
			},
		},
	},
	{
		.type			= stb_traits_type_string_enum,
		.description	= "Picture dimensions",
		.id				= "size",
		.settable		= false,
		.api_data		= 0,
		.value			=
		{
			.string_enum_type =
			{
				.default_value	= "416x224",
				.enum_values	= 
				{
					"416x224", 0,
				},
			},
		},
	},
	{
		.type			= stb_traits_type_string_enum,
		.description	= "Video codec profile",
		.id				= "profile",
		.settable		= false,
		.api_data		= 0,
		.value			=
		{
			.string_enum_type =
			{
				.default_value	= "constrained baseline",
				.enum_values	= 
				{
					"constrained baseline", 0,
				},
			},
		},
	},
	{
		.type			= stb_traits_type_string_enum,
		.description	= "Video codec level",
		.id				= "level",
		.settable		= false,
		.api_data		= 0,
		.value			=
		{
			.string_enum_type =
			{
				.default_value	= "3.0",
				.enum_values	= 
				{
					"3.0", 0,
				},
			},
		},
	},
	{
		.type			= stb_traits_type_int,
		.description	= "Max. bidirectional frames in a row",
		.id				= "bframes",
		.settable		= false,
		.api_data		= 0,
		.value			=
		{
			.int_type =
			{
				.default_value	= 0,
				.min_value		= 0,
				.max_value		= 0,
				.scaling_factor	= 1,
			},
		},
	},
	{
		.type			= stb_traits_type_string_enum,
		.description	= "Frame rate",
		.id				= "framerate",
		.settable		= true,
		.api_data		= "framerate",
		.value			=
		{
			.string_enum_type =
			{
				.default_value	= "30",
				.enum_values	= 
				{
					"24", "25", "30", 0,
				},
			},
		},
	},
};

static const stb_feature_t features_vuduo2[] =
{
	{
		.type			= stb_traits_type_string_enum,
		.description	= "Video codec",
		.id				= "vcodec",
		.settable		= true,
		.api_data		= "video_codec",
		.value			=
		{
			.string_enum_type =
			{
				.default_value	= "h264",
				.enum_values	= 
				{
					"h264", 0,
				},
			},
		},
	},
	{
		.type			= stb_traits_type_string_enum,
		.description	= "Audio codec",
		.id				= "acodec",
		.settable		= true,
		.api_data		= "audio_codec",
		.value			=
		{
			.string_enum_type =
			{
				.default_value	= "aac",
				.enum_values	= 
				{
					"aac", 0,
				},
			},
		},
	},
	{
		.type			= stb_traits_type_int,
		.description	= "Total stream bit rate",
		.id				= "bitrate",
		.settable		= true,
		.api_data		= "bitrate",
		.value			=
		{
			.int_type =
			{
				.default_value	= 1000,
				.min_value		= 100,
				.max_value		= 10000,
				.scaling_factor	= 1000,
			},
		},
	},
	{
		.type			= stb_traits_type_string_enum,
		.description	= "Picture dimensions",
		.id				= "size",
		.settable		= true,
		.api_data		= "display_format",
		.value			=
		{
			.string_enum_type =
			{
				.default_value	= "480p",
				.enum_values	= 
				{
					"480p", "576p", "720p", 0,
				},
			},
		},
	},
	{
		.type			= stb_traits_type_string_enum,
		.description	= "Video codec profile",
		.id				= "profile",
		.settable		= true,
		.api_data		= "profile",
		.value			=
		{
			.string_enum_type =
			{
				.default_value	= "baseline",
				.enum_values	= 
				{
					"baseline", "simple", "main", "high", 0,
				},
			},
		},
	},
	{
		.type			= stb_traits_type_string_enum,
		.description	= "Video codec level",
		.id				= "level",
		.settable		= true,
		.api_data		= "level",
		.value			=
		{
			.string_enum_type =
			{
				.default_value	= "3.1",
				.enum_values	= 
				{
					"3.1", "3.2", "4.0", 0,
				},
			},
		},
	},
	{
		.type			= stb_traits_type_int,
		.description	= "Max. bidirectional frames in a row",
		.id				= "bframes",
		.settable		= true,
		.api_data		= "gop_frameb",
		.value			=
		{
			.int_type =
			{
				.default_value	= 0,
				.min_value		= 0,
				.max_value		= 2,
				.scaling_factor	= 1,
			},
		},
	},
	{
		.type			= stb_traits_type_string_enum,
		.description	= "Frame rate",
		.id				= "framerate",
		.settable		= true,
		.api_data		= "framerate",
		.value			=
		{
			.string_enum_type =
			{
				.default_value	= "30000",
				.enum_values	= 
				{
					"24000", "25000", "30000", 0,
				},
			},
		},
	},
};

static const stb_feature_t features_generic[] =
{
};

const stbs_traits_t stbs_traits =
{
	.num_traits = 3,
	.traits_entry =
	{
		{
			.manufacturer	= "VU+",
			.model			= "Solo2",
			.chipset		= "bcm7356",
			.encoders		= 1,
			.num_id			= 2,
			.id				=
			{
				{ "/proc/stb/info/model", "dm8000" },
				{ "/proc/stb/info/vumodel", "solo2" },
			},
			.num_features	= 8,
			.features		= features_vusolo2,
		},

		{
			.manufacturer	= "VU+",
			.model			= "Duo2",
			.chipset		= "bcm7424",
			.encoders		= 2,
			.num_id			= 2,
			.id				=
			{
				{ "/proc/stb/info/model", "dm8000" },
				{ "/proc/stb/info/vumodel", "duo2" },
			},
			.num_features	= 8,
			.features		= features_vuduo2,
		},
		{
			.manufacturer	= "generic",
			.model			= "non-transcoding settopbox",
			.chipset		= "unknown",
			.encoders		= 0,
			.num_id			= 0,
			.id				=
			{
			},
			.num_features	= 0,
			.features		= features_generic,
		},
	},
};
