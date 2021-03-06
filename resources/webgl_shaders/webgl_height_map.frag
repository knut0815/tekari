precision highp float;

uniform bool use_diffuse_shading;
uniform bool use_specular_shading;
uniform bool use_integrated_colors;

uniform sampler2D color_map;

varying float height;
varying vec3 position;
varying vec3 normal;
varying vec3 integrated_color;

const vec3 light_color = vec3(0.8, 0.8, 0.8);

void main() {
    vec3 color = use_integrated_colors ? integrated_color : texture2D(color_map, vec2(height, 0)).rgb;

	vec3 pos_to_light = normalize(vec3(0,0,4) - position);
	float specular = clamp(dot(reflect(-pos_to_light, normal), pos_to_light), 0.0, 1.0);
	vec3 out_color3 = vec3(0.0);
	out_color3 += float(use_diffuse_shading) * (0.2 + abs(dot(pos_to_light, normal))) * color * light_color;
	out_color3 += float(use_diffuse_shading && use_specular_shading) * pow(specular, 20.0) * light_color;
	out_color3 += float(!use_diffuse_shading) * color;

	gl_FragColor = vec4(out_color3, 1.0);
}
