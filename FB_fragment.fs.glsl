#version 420 core

uniform sampler2D tex;
//layout (binding = 0) uniform sampler2D tex;
//layout (binding = 1) uniform sampler2D blur_tex;

out vec4 color;

uniform int shader_index;
uniform float elapsedTime;

in VS_OUT
{
	vec2 texcoord;
} fs_in;


float sigma_e = 2.0f;
float sigma_r = 2.8f;
float phi = 3.1415926f;
float tau = 0.99f;
float twoSigmaESquared = 2.0 * sigma_e * sigma_e;		
float twoSigmaRSquared = 2.0 * sigma_r * sigma_r;		
int halfWidth = int(ceil( 2.0 * sigma_r ));
vec2 img_size = vec2(600,600);
int nbins = 8;

float contrast = 0.5;
float intensityAdjust = 1;
float noiseAmplification = 1;
float bufferAmplication = 1;

float rnd_factor = 0.05;
float rnd_scale = 5.1;
vec2 v1 = vec2(92., 80.);
vec2 v2 = vec2(41., 62.);
float rand(vec2 co)
{
	return fract(sin(dot(co.xy, v1)) + cos(dot(co.xy, v2)) * rnd_scale);
}

/*
vec4 spline(float x, vec4 c1, vec4 c2, vec4 c3, vec4 c4, vec4 c5, vec4 c6, vec4 c7, vec4 c8, vec4 c9)
{
	float w1, w2, w3, w4, w5, w6, w7, w8, w9;
	w1 = 0.0; w2 = 0.0; w3 = 0.0; w4 = 0.0; w5 = 0.0;
	w6 = 0.0; w7 = 0.0; w8 = 0.0; w9 = 0.0;
	float tmp = x * 8.0;
	if (tmp <= 1.0) {
		w1 = 1.0 - tmp;
		w2 = tmp;
	}
	else if (tmp <= 2.0) {
		tmp = tmp - 1.0;
		w2 = 1.0 - tmp;
		w3 = tmp;
	}
	else if (tmp <= 3.0) {
		tmp = tmp - 2.0;
		w3 = 1.0 - tmp;
		w4 = tmp;
	}
	else if (tmp <= 4.0) {
		tmp = tmp - 3.0;
		w4 = 1.0 - tmp;
		w5 = tmp;
	}
	else if (tmp <= 5.0) {
		tmp = tmp - 4.0;
		w5 = 1.0 - tmp;
		w6 = tmp;
	}
	else if (tmp <= 6.0) {
		tmp = tmp - 5.0;
		w6 = 1.0 - tmp;
		w7 = tmp;
	}
	else if (tmp <= 7.0) {
		tmp = tmp - 6.0;
		w7 = 1.0 - tmp;
		w8 = tmp;
	}
	else
	{
		tmp = clamp(tmp - 7.0, 0.0, 1.0);
		w8 = 1.0 - tmp;
		w9 = tmp;
	}
	return w1 * c1 + w2 * c2 + w3 * c3 + w4 * c4 + w5 * c5 + w6 * c6 + w7 * c7 + w8 * c8 + w9 * c9;
}
*/

void main(void)
{
	color = texture(tex, fs_in.texcoord);

	// Abstraction
	if (shader_index == 0) {
		color = texture(tex, fs_in.texcoord);
	}
	else if (shader_index == 1) {
		color = vec4(0);
		//quantization
		
		vec4 texture_color = texture(tex, fs_in.texcoord);
		float r = floor(texture_color.r * float(nbins)) / float(nbins);
		float g = floor(texture_color.g * float(nbins)) / float(nbins);
		float b = floor(texture_color.b * float(nbins)) / float(nbins); 

		//DOG
		vec2 sum = vec2(0.0);
		vec2 norm = vec2(0.0);

		int kernel_count = 0;
		for ( int i = -halfWidth; i <= halfWidth; ++i ) {
			for ( int j = -halfWidth; j <= halfWidth; ++j ) {
				float d = length(vec2(i,j));
				vec2 kernel = vec2( exp( -d * d / twoSigmaESquared ), 
				exp( -d * d / twoSigmaRSquared ));
				vec4 c = texture(tex, fs_in.texcoord + vec2(i,j) / img_size);
				vec2 L = vec2(0.299 * c.r + 0.587 * c.g + 0.114 * c.b);
				norm += 2.0 * kernel;
				sum += kernel * L;
			}
		}
		sum /= norm;
		float H = 100.0 * (sum.x - tau * sum.y);
		float edge = ( H > 0.0 )? 1.0 : 2.0 * smoothstep(-2.0, 2.0, phi * H );
		
		//blur
		int n = 0;
		int half_size = 3;
		for (int i = -half_size; i <= half_size; ++i) {
			for (int j = -half_size; j <= half_size; ++j) {
				vec4 c = texture(tex, fs_in.texcoord + vec2(i, j) / img_size);
				color += c;
				n++;
			}
		}
		color /= n;

		//quantization
		color *= vec4(r, g, b, texture_color.a);
		//DOG
		color *= vec4(edge, edge, edge, 1.0);
	}
	
	// LaplacianFilter
	if (shader_index == 2) {
		vec4 texture_color = texture(tex, fs_in.texcoord);

		color = vec4(0);

		float grayscale_color = 
			0.2126 * texture_color.r + 
			0.7152 * texture_color.g + 
			0.0722 * texture_color.b;
		

		float threshold = 127.5;

		int half_size = 1;

		for (int i = -half_size; i <= half_size; ++i) {
			for (int j = -half_size; j <= half_size; ++j) {
				vec4 c = texture(tex, fs_in.texcoord + vec2(i, j) / img_size);
				if (i == 0 && j == 0) {
					if (c.r > threshold) {
						c.r = 255.0;
					}
					else {
						c.r = 0.0;
					}
					if (c.g > threshold) {
						c.g = 255.0;
					}
					else{
						c.r = 0.0;
					}
					if (c.b > threshold) {
						c.g = 255.0;
					}
					else {
						c.b = 0.0;
					}
					float s_color = c.r + c.g + c.b;
					color += s_color * 8;
				}
				else {
					if (c.r > threshold) {
						c.r = 255.0;
					}
					else {
						c.r = 0.0;
					}
					if (c.g > threshold) {
						c.g = 255.0;
					}
					else {
						c.r = 0.0;
					}
					if (c.b > threshold) {
						c.g = 255.0;
					}
					else {
						c.b = 0.0;
					}
					float s_color = c.r + c.g + c.b;
					color -= s_color ;
					//color -= c;
				}
			}
		}

		color *= vec4(grayscale_color, grayscale_color, grayscale_color, 1.0);

	}
	// SharpnessFilter
	else if (shader_index == 3) {
		color = vec4(0);

		int half_size = 1;
		
		for (int i = -half_size; i <= half_size; ++i) {
			for (int j = -half_size; j <= half_size; ++j) {
				vec4 c = texture(tex, fs_in.texcoord + vec2(i, j) / img_size);
				if (i == 0 && j == 0) {
					color += c * 9;
				}
				else {
					color -= c;
				}
			}
		}
		
	}
	// Pixelation
	else if (shader_index == 4) {
		
		float Pixels = 1024.0;
		float dx = 15.0 * (1.0 / Pixels);
		float dy = 10.0 * (1.0 / Pixels);
		vec2 Coord = vec2(
			dx * floor(fs_in.texcoord.x / dx),
			dy * floor(fs_in.texcoord.y / dy));

		color = texture(tex, Coord);

	}
	// Fish_eyedistortion
	else if (shader_index == 5) {

		float aperture = 178.0;
		float apertureHalf = 0.5 * aperture * (phi / 180.0);
		float maxFactor = sin(apertureHalf);

		vec2 uv;
		vec2 xy = 2.0 * fs_in.texcoord.xy - 1.0;
		float d = length(xy);
		if (d < (2.0 - maxFactor))
		{
			d = length(xy * maxFactor);
			float z = sqrt(1.0 - d * d);
			float r = atan(d, z) / phi;
			float pi = atan(xy.y, xy.x);

			uv.x = r * cos(pi) + 0.5;
			uv.y = r * sin(pi) + 0.5;
		}
		else
		{
			uv = fs_in.texcoord.xy;
		}
		vec4 c = texture(tex,uv);
		color = c;
	}
	// Red-Blue Stereo
	else if (shader_index == 6) {
		float offset = 0.005;
		vec4 texture_color_Left = texture(tex, fs_in.texcoord - vec2(offset, 0.0));
		vec4 texture_color_Right = texture(tex, fs_in.texcoord + vec2(offset, 0.0));
		vec4 texture_color = vec4(
			texture_color_Left.r*0.29 + texture_color_Left.g*0.58 + texture_color_Left.b*0.114,
			texture_color_Right.g,
			texture_color_Right.b, 0.0);
		color = texture_color;
	}
	// Bloom_Effect
	else if (shader_index == 7) {
		color = vec4(0);

		//blur
		int n = 0;
		int half_size = 3;
		for (int i = -half_size; i <= half_size; ++i) {
			for (int j = -half_size; j <= half_size; ++j) {
				vec4 c = texture(tex, fs_in.texcoord + vec2(i, j) / img_size);
				color += c;
				n++;
			}
		}
		color /= n;

		vec4 texture_color = texture(tex, fs_in.texcoord);

		color = color * 0.6 + texture_color  ;
	}
	// halftoning
	else if (shader_index == 8) {
		float frequency = 250.0;
		vec2 st2 = mat2(0.707, -0.707, 0.707, 0.707) * fs_in.texcoord;
		vec2 nearest = 2.0*fract(frequency * st2) - 1.0;
		float dist = length(nearest);
		vec3 texcolor = texture(tex, fs_in.texcoord).rgb;
		float radius = sqrt(1.0- texcolor.g);
		vec3 white = vec3(1.0, 1.0, 1.0);
		vec3 black = vec3(0.0, 0.0, 0.0);
		vec3 fragcolor = mix(black, white, step(radius, dist));
		color = vec4(fragcolor, 1.0);
	}
	// NightVision
	else if (shader_index == 9) {

		vec2 uv;
		uv.x = 0.35*sin(elapsedTime*50.0);
		uv.y = 0.35*cos(elapsedTime*50.0);
		color = vec4(0);

		vec3 noise = texture(tex, fs_in.texcoord + uv).rgb * noiseAmplification;
		vec3 texcolor = texture(tex, fs_in.texcoord).rgb;
		vec3 sceneColor = texture(tex, fs_in.texcoord + (noise.xy*0.005)).rgb * bufferAmplication;

		vec3 lumvec = vec3(0.30, 0.59, 0.11);
		
		vec3 visionColor = vec3(0.1, 0.95, 0.2);

		color.rgb = (sceneColor + (noise*0.2)) * visionColor * texcolor;
		color.a = 1.0;
		
		vec2 lensRadius ;
		lensRadius.x = 0.45;
		lensRadius.y = 0.38;

		//Lens Circle
		vec4 texturecolor = texture(tex, fs_in.texcoord.xy);
		float dist = distance(fs_in.texcoord.xy, vec2(0.5, 0.5));

		texturecolor.rgb *= smoothstep(lensRadius.x, lensRadius.y, dist);
		color *= texturecolor;

	}
	// Frosted Glass
	else if (shader_index == 10) {
		
		vec2 uv = fs_in.texcoord.xy;
		vec3 tc = vec3(1.0, 0.0, 0.0);

		vec2 rnd = vec2(rand(uv.xy), rand(uv.yx));
		tc = texture(tex, uv + rnd * rnd_factor).rgb;

		color = vec4(tc, 1.0);

		/*
		float Pixels = 1024.0;
		float Freq = 0.3;

		float dx = 15.0 * (1.0 / Pixels);
		float dy = 10.0 * (1.0 / Pixels);
		vec2 ox = vec2(dx, 0.0);
		vec2 oy = vec2(0.0, dy);
		vec2 PP = uv - oy;
		vec4 C00 = texture(tex, PP - ox);
		vec4 C01 = texture(tex, PP);
		vec4 C02 = texture(tex, PP + ox);
		PP = uv;
		vec4 C10 = texture(tex, PP - ox);
		vec4 C11 = texture(tex, PP);
		vec4 C12 = texture(tex, PP + ox);
		PP = uv + oy;
		vec4 C20 = texture(tex, PP - ox);
		vec4 C21 = texture(tex, PP);
		vec4 C22 = texture(tex, PP + ox);

		float n = texture(tex, Freq * uv).x;
		n = mod(n, 0.111111) / 0.111111;
		vec4 result = spline(n, C00, C01, C02, C10, C11, C12, C20, C21, C22);
		tc = result.rgb;

		color = vec4(tc, 1.0);
		*/
		
	}
	// Swirl
	else if (shader_index == 11) {
	
		float radius = 120.0;
		float angle = 1.0;
		vec2 center = vec2(150.0, 150.0);

		vec2 uv = fs_in.texcoord.xy;
		vec2 texSize = vec2(300, 300);
		vec2 tc = uv * texSize;
		tc -= center;
		float dist = length(tc);
		if (dist < radius)
		{
			float percent = (radius - dist) / radius;
			float theta = percent * percent * angle * 8.0;
			float s = sin(theta);
			float c = cos(theta);
			tc = vec2(dot(tc, vec2(c, -s)), dot(tc, vec2(s, c)));
		}
		tc += center;
		vec3 tcolor = texture(tex, tc / texSize).rgb;
		color = vec4(tcolor, 1.0);
	
	}
}