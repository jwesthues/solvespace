//-----------------------------------------------------------------------------
// Indexed Mesh rendering shader
//
// Copyright 2016 Aleksey Egorov
//-----------------------------------------------------------------------------
uniform vec4 color;
uniform sampler2D texture;

void main() {
    if(texture2D(texture, gl_FragCoord.xy / 32.0).a < 0.5) discard;
    gl_FragColor = color;
}
