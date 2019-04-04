// gpgpu FBO -- pix
#version 330 core

//-- 片段着色器的 主输出：颜色
out vec4 FragColor;

//-- in: 从 顶点着色器 传来的 数据（名称和类型 都要相同）
in vec2 TexCoord;      //-- 每个pix 在 tecture 上的坐标 [0.0,1.0]-[left_bottom]
in vec2 u_resolution;  //-- 每个pix 在 window 上的坐标  [-1.0,1.0]


//-- sampler 系列 是 采样器数据类型。
//-- 现在，我们创建了 1个 纹理采样器
uniform sampler2D texture1; //- 用不到

uniform float u_time; //-- glfwGetTime(); 未做缩放。
                      // 在一些 必须 [0.0, 1.0] 的使用场合中，
                      // 通过 abs(sin(u_time)) 来转换

//---- 用户定制 uniforms -----
uniform vec2 canvasCFPos; //- 以 chunk 为单位的, canvas左下角 cfpox

//- 对于每个游戏存档，这组值是静态的，只需要传入一次...
uniform float SCR_WIDTH;
uniform float SCR_HEIGHT;
uniform vec2  altiSeed_pposOffSeaLvl;
uniform vec2  altiSeed_pposOffBig;
uniform vec2  altiSeed_pposOffMid;
uniform vec2  altiSeed_pposOffSml;


//============ vals ===========//
vec2 lb;      //- [0.0 ,1.0]-[left_bottom]
vec2 lbAlign; //- 对齐过的 lb，首先，校准了 y值比例（毕竟window可能不是正方形）
              //  其次，1个单位 lbAlign，等于 chunk-ppos 尺寸，而不是 canvas尺寸

vec2 mid = vec2(0.4, 0.4); //- 和 lb 同体系，canvas中心坐标
//-------

vec3 color;

float PIXES_PER_CHUNK = 256.0; //- tmp

//- 在未来，freq 这组值也会收到 ecosys 影响 --
float freqSeaLvl = 0.05;
float freqBig = 0.4;
float freqMid = 1.6;
float freqSml = 4.0;
float freqAnim = 5.0;

float zOffBig = 0.2;
float zOffMid = 7.5;
float zOffSml = 17.8;

float XYScale = 0.9; //- 将 simplex-noise 压扁些
                     // 这个值可能被丢弃...


float seaLvl;  //- 海平面。 值越小，land区越大。通过平滑曲线生成
                      

//============ funcs ===========//
//-- qualifer/限定符：
// in     -- 只读
// out    -- 只写
// inout  -- 可读可写
void prepare();
float simplex_noise2( float _x, float _y );
float simplex_noise2( vec2 v );
float simplex_noise3( float _x, float _y, float _z );


/* ====================================================
 *                      main
 * ----------------------------------------------------
 */
void main()
{
    prepare();
    
    //------------------//
    //     time
    //------------------//
    float tm = u_time * 0.18;
        //- 理想的 time 值是一个 在 [0.0, n.0] 之间来回运动的值。 

    //------------------//
    // pixCFPos: 以 chunk 为晶格的 fpos
    vec2 pixCFPos = canvasCFPos + lbAlign;

    pixCFPos *= 240.0;
    pixCFPos = floor(pixCFPos);
    pixCFPos =  pixCFPos / 240.0;
    

    //------------------//
    //     seaLvl
    //------------------//
    float pixDistance = length( pixCFPos - 0 );//- 暂时假设，(0,0) 为世界中心
    pixDistance /= 10.0; //- 每10个chunk，递增一级
    //-------
    seaLvl = simplex_noise2( (pixCFPos ) * freqSeaLvl ) * 45.0; // [-100.0, 100.0]
    seaLvl += pixDistance;
    if( seaLvl < 0.0 ){ //- land
        seaLvl *= 0.15;  // [-15.0, 100.0]
    }

    //------------------//
    //    alti.val
    //------------------//
    //--- 使用速度最快的 2D-simplex-noise ---
    vec2 tmpV2;
    tmpV2 = (pixCFPos + altiSeed_pposOffBig) * freqBig;
    tmpV2.x *= XYScale;
    float pnValBig = simplex_noise2( tmpV2 ) * 100.0 - seaLvl; // [-100.0, 100.0]

    tmpV2 = (pixCFPos + altiSeed_pposOffMid) * freqMid;
    tmpV2.x *= XYScale;
    float pnValMid = simplex_noise2( tmpV2 ) * 50.0  - seaLvl; // [-50.0, 50.0]

    tmpV2 = (pixCFPos + altiSeed_pposOffSml) * freqSml;
    tmpV2.x *= XYScale;
    float pnValSml = simplex_noise2( tmpV2 ) * 20.0  - seaLvl; // [-20.0, 20.0]

    tmpV2 =  pixCFPos * freqAnim;
    tmpV2.x *= XYScale;
    float pnValAnim = simplex_noise3( tmpV2.x, tmpV2.y, tm );
    //-----------------

    pnValAnim = (pnValAnim - 1.0) * 10.0 - 10.0; //- 只有负值

    float altiVal = floor(pnValBig + pnValMid + pnValSml);

    if( altiVal < 0.0 ){ //- only water anim
        altiVal += pnValAnim;
    }

    //------- 抹平头尾 -------//
    if( altiVal > 100.0 ){
        altiVal = 100.0;
    }else if( altiVal < -100.0 ){
        altiVal = -100.0;
    }
    // now, altiVal: [-100.0, 100.0]

    //------------------//
    //      lvl
    //------------------//
    float altiLvl;
    if( altiVal < 0.0 ){ //- under water

        //------ -1 -------
        if( altiVal > -10.0 ){
            altiLvl = -1.0;
        }
        //------ -2 -------
        else if( altiVal > -30.0 ){
            altiLvl = -2.0;
        }

        //------ -3 -------
        else if( altiVal > -50.0 ){
            altiLvl = -3.0;
        }

        //------ -4 -------
        else if( altiVal > -60.0 ){
            altiLvl = -4.0;
        }

        //------ -5 -------
        else if( altiVal > -90.0 ){
            altiLvl = -5.0;
        }

        //------ -6 -------
        else{
            altiLvl = -6.0;
        }


        //----------------
        if( altiLvl < -6.0 ){
            altiLvl = -6.0;
        }
    }else{ //- land
        altiLvl = 1.0;
    }

    //------------------//
    //       color
    //------------------//
    if( altiLvl > -1.0 ){ //- land
        //color = vec3( 0.24, 0.36, 0.44  );
        //color = vec3( 0.07, 0.59, 0.8  );
        color = vec3( 0.18, 0.32, 0.38  );
    }else if( altiLvl == -1.0 ){
        //color = vec3( 0.24, 0.36, 0.44  );
        //color = vec3( 0.07, 0.59, 0.8  );
        color = vec3( 0.18, 0.32, 0.38  );
    }else if( altiLvl == -2.0 ){
        color = vec3( 0.18, 0.32, 0.38  );
        //color = vec3( 0.2, 0.4, 0.6  );
    }else if( altiLvl == -3.0 ){
        color = vec3( 0.16, 0.26, 0.34  );
        //color = vec3( 0.50, 0.50, 0.50  );
    }else if( altiLvl == -4.0 ){
        color = vec3( 0.14, 0.22, 0.30  );
        //color = vec3( 0.70, 0.70, 0.70  );
    }else if( altiLvl == -5.0 ){
        color = vec3( 0.12, 0.20, 0.26  );
    }else{ 
        color = vec3( 0.10, 0.18, 0.22  );
    }

    //------------------//
    //   distance: lb -> mid
    //------------------//
    float distanceMid = length(lb - mid );
    distanceMid *= distanceMid;

    float alpha = 1.0 - 10.0*distanceMid;
    if( alpha < 0.0 ){
        alpha = 0.0;
    }


    FragColor = vec4( color, 1.0 );
    //FragColor = vec4( color, 0.0 );
    //FragColor = vec4( color, alpha ); //- rgba.alpha MUST be 1.0 !!!
    //FragColor = vec4( lb.xxy, 1.0 ); //- rgba.alpha MUST be 1.0 !!!
}


/* ====================================================
 *                     prepare
 * ----------------------------------------------------
 * -- 准备工作
 */
void prepare(){
    // TexCoord:      每个pix 在 tecture 上的坐标 [0.0,1.0]
    // u_resolution:  每个pix 在 window 上的坐标  [-1.0,1.0]
    //--------------------------//
    //    初始化 lb, lt
    //--------------------------//
    //-- 左下坐标系 [0,1]
    lb = TexCoord;
    //lb.y *= 0.5;
    //---
    lbAlign = lb;
    lbAlign.y *= SCR_HEIGHT/SCR_WIDTH;
    lbAlign *= SCR_WIDTH/PIXES_PER_CHUNK;
}



/* ====================================================
 *                 Simplex Noise
 * ----------------------------------------------------
 *  Description : Array and textureless GLSL 2D/3D/4D simplex noise functions.
 *       Author : Ian McEwan, Ashima Arts.     
 *   Maintainer : stegu
 *      Lastmod : 20110822 (ijm)
 *      License : Copyright (C) 2011 Ashima Arts. All rights reserved.
 *                Distributed under the MIT License. See LICENSE file.
 *                https://github.com/ashima/webgl-noise
 *                https://github.com/stegu/webgl-noise
 */
vec2 mod289V2(vec2 x){ return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec3 mod289V3(vec3 x){ return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec4 mod289V4(vec4 x){ return x - floor(x * (1.0 / 289.0)) * 289.0; }
vec3 permuteV3(vec3 x){ return mod289V3(((x*34.0)+1.0)*x); }
vec4 permuteV4(vec4 x){ return mod289V4(((x*34.0)+1.0)*x); }
vec4 taylorInvSqrtV4(vec4 r){ return 1.79284291400159 - 0.85373472095314 * r; }

//------------------- ret: [-1.0, 1.0] ---------------------
float simplex_noise2( float _x, float _y ){
    return simplex_noise2( vec2(_x, _y) );
}

//------------------- ret: [-1.0, 1.0] ---------------------
float simplex_noise2( vec2 v ){
  const vec4 C = vec4(0.211324865405187,  // (3.0-sqrt(3.0))/6.0
                      0.366025403784439,  // 0.5*(sqrt(3.0)-1.0)
                     -0.577350269189626,  // -1.0 + 2.0 * C.x
                      0.024390243902439); // 1.0 / 41.0
  // First corner
  vec2 i  = floor(v + dot(v, C.yy) );
  vec2 x0 = v -   i + dot(i, C.xx);
  // Other corners
  vec2 i1;
  //i1.x = step( x0.y, x0.x ); // x0.x > x0.y ? 1.0 : 0.0
  //i1.y = 1.0 - i1.x;
  i1 = (x0.x > x0.y) ? vec2(1.0, 0.0) : vec2(0.0, 1.0);
  // x0 = x0 - 0.0 + 0.0 * C.xx ;
  // x1 = x0 - i1 + 1.0 * C.xx ;
  // x2 = x0 - 1.0 + 2.0 * C.xx ;
  vec4 x12 = x0.xyxy + C.xxzz;
  x12.xy -= i1;
  // Permutations
  i = mod289V2(i); // Avoid truncation effects in permutation
  vec3 p = permuteV3( permuteV3( i.y + vec3(0.0, i1.y, 1.0 ))
		+ i.x + vec3(0.0, i1.x, 1.0 ));
  vec3 m = max(0.5 - vec3(dot(x0,x0), dot(x12.xy,x12.xy), dot(x12.zw,x12.zw)), 0.0);
  m = m*m ;
  m = m*m ;
  // Gradients: 41 points uniformly over a line, mapped onto a diamond.
  // The ring size 17*17 = 289 is close to a multiple of 41 (41*7 = 287)
  vec3 x = 2.0 * fract(p * C.www) - 1.0;
  vec3 h = abs(x) - 0.5;
  vec3 ox = floor(x + 0.5);
  vec3 a0 = x - ox;
  // Normalise gradients implicitly by scaling m
  // Approximation of: m *= inversesqrt( a0*a0 + h*h );
  m *= 1.79284291400159 - 0.85373472095314 * ( a0*a0 + h*h );
  // Compute final noise value at P
  vec3 g;
  g.x  = a0.x  * x0.x  + h.x  * x0.y;
  g.yz = a0.yz * x12.xz + h.yz * x12.yw;
  return 130.0 * dot(m, g);
}


//------------------- ret: [-1.0, 1.0] ---------------------
float simplex_noise3( float _x, float _y, float _z ){ 
  vec3 v = vec3( _x, _y, _z );
  const vec2  C = vec2(1.0/6.0, 1.0/3.0) ;
  const vec4  D = vec4(0.0, 0.5, 1.0, 2.0);
  // First corner
  vec3 i  = floor(v + dot(v, C.yyy) );
  vec3 x0 =   v - i + dot(i, C.xxx) ;
  // Other corners
  vec3 g = step(x0.yzx, x0.xyz);
  vec3 l = 1.0 - g;
  vec3 i1 = min( g.xyz, l.zxy );
  vec3 i2 = max( g.xyz, l.zxy );
  //   x0 = x0 - 0.0 + 0.0 * C.xxx;
  //   x1 = x0 - i1  + 1.0 * C.xxx;
  //   x2 = x0 - i2  + 2.0 * C.xxx;
  //   x3 = x0 - 1.0 + 3.0 * C.xxx;
  vec3 x1 = x0 - i1 + C.xxx;
  vec3 x2 = x0 - i2 + C.yyy; // 2.0*C.x = 1/3 = C.y
  vec3 x3 = x0 - D.yyy;      // -1.0+3.0*C.x = -0.5 = -D.y
  // Permutations
  i = mod289V3(i); 
  vec4 p = permuteV4( permuteV4( permuteV4( 
             i.z + vec4(0.0, i1.z, i2.z, 1.0 ))
           + i.y + vec4(0.0, i1.y, i2.y, 1.0 )) 
           + i.x + vec4(0.0, i1.x, i2.x, 1.0 ));
  // Gradients: 7x7 points over a square, mapped onto an octahedron.
  // The ring size 17*17 = 289 is close to a multiple of 49 (49*6 = 294)
  float n_ = 0.142857142857; // 1.0/7.0
  vec3  ns = n_ * D.wyz - D.xzx;

  vec4 j = p - 49.0 * floor(p * ns.z * ns.z);  //  mod(p,7*7)

  vec4 x_ = floor(j * ns.z);
  vec4 y_ = floor(j - 7.0 * x_ );    // mod(j,N)

  vec4 x = x_ *ns.x + ns.yyyy;
  vec4 y = y_ *ns.x + ns.yyyy;
  vec4 h = 1.0 - abs(x) - abs(y);

  vec4 b0 = vec4( x.xy, y.xy );
  vec4 b1 = vec4( x.zw, y.zw );

  //vec4 s0 = vec4(lessThan(b0,0.0))*2.0 - 1.0;
  //vec4 s1 = vec4(lessThan(b1,0.0))*2.0 - 1.0;
  vec4 s0 = floor(b0)*2.0 + 1.0;
  vec4 s1 = floor(b1)*2.0 + 1.0;
  vec4 sh = -step(h, vec4(0.0));

  vec4 a0 = b0.xzyw + s0.xzyw*sh.xxyy ;
  vec4 a1 = b1.xzyw + s1.xzyw*sh.zzww ;

  vec3 p0 = vec3(a0.xy,h.x);
  vec3 p1 = vec3(a0.zw,h.y);
  vec3 p2 = vec3(a1.xy,h.z);
  vec3 p3 = vec3(a1.zw,h.w);
  //Normalise gradients
  vec4 norm = taylorInvSqrtV4(vec4(dot(p0,p0), dot(p1,p1), dot(p2, p2), dot(p3,p3)));
  p0 *= norm.x;
  p1 *= norm.y;
  p2 *= norm.z;
  p3 *= norm.w;
  // Mix final noise value
  vec4 m = max(0.6 - vec4(dot(x0,x0), dot(x1,x1), dot(x2,x2), dot(x3,x3)), 0.0);
  m = m * m;
  return 42.0 * dot( m*m, vec4( dot(p0,x0), dot(p1,x1), 
                                dot(p2,x2), dot(p3,x3) ) );
}


