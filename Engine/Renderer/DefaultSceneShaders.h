#pragma once

namespace NoJob::DefaultSceneShaders
{
    // Shared default PBR scene shader used by both the Editor and Runtime.
    // Keeping a single canonical source prevents the two executables from
    // silently drifting apart when renderer features are added.
    inline constexpr const char* Vertex = R"(
            #version 460 core

            layout(location = 0) in vec3 a_Position;
            layout(location = 1) in vec3 a_Normal;
            layout(location = 2) in vec2 a_TexCoord;
            layout(location = 3) in vec4 a_BoneIDs;
            layout(location = 4) in vec4 a_BoneWeights;

            uniform mat4 u_Transform;
            uniform int u_UseSkinning;
            uniform mat4 u_Bones[128];
            uniform mat4 u_ViewProjection;

            out vec3 v_WorldPosition;
            out vec3 v_Normal;
            out vec2 v_TexCoord;

            void main()
            {
                vec4 localPosition = vec4(a_Position, 1.0);
                vec3 localNormal = a_Normal;
                if (u_UseSkinning == 1 && dot(a_BoneWeights, vec4(1.0)) > 0.0)
                {
                    ivec4 ids = ivec4(a_BoneIDs);
                    mat4 skin =
                        u_Bones[ids.x] * a_BoneWeights.x +
                        u_Bones[ids.y] * a_BoneWeights.y +
                        u_Bones[ids.z] * a_BoneWeights.z +
                        u_Bones[ids.w] * a_BoneWeights.w;
                    localPosition = skin * localPosition;
                    localNormal = normalize(mat3(skin) * a_Normal);
                }

                vec4 worldPosition = u_Transform * localPosition;

                v_WorldPosition = worldPosition.xyz;
                v_Normal =
                    mat3(transpose(inverse(u_Transform))) * localNormal;
                v_TexCoord = a_TexCoord;

                gl_Position =
                    u_ViewProjection * worldPosition;
            }
        )";

    inline constexpr const char* Fragment = R"(
            #version 460 core
            layout(location=0) out vec4 o_Color;
            in vec3 v_WorldPosition; in vec3 v_Normal; in vec2 v_TexCoord;

            uniform vec4 u_Color;
            uniform sampler2D u_Texture;
            uniform int u_UseTexture;
            uniform int u_SurfaceMode;
            uniform float u_AlphaCutoff;
            uniform sampler2D u_NormalMap; uniform int u_UseNormalMap; uniform float u_NormalStrength;
            uniform sampler2D u_MetallicMap; uniform int u_UseMetallicMap;
            uniform sampler2D u_RoughnessMap; uniform int u_UseRoughnessMap;
            uniform sampler2D u_AOMap; uniform int u_UseAOMap;
            uniform sampler2D u_EmissiveMap; uniform int u_UseEmissiveMap;
            uniform vec3 u_EmissiveColor; uniform float u_EmissiveStrength;
            uniform vec3 u_ViewPosition; uniform vec3 u_AmbientColor;
            uniform int u_UseIBL;
            uniform float u_EnvironmentIntensity;
            uniform float u_DiffuseIBLStrength;
            uniform float u_SpecularIBLStrength;
            uniform float u_EnvironmentRotation;
            uniform int u_HasHDRI;
            uniform samplerCube u_IrradianceMap;
            uniform samplerCube u_PrefilterMap;
            uniform sampler2D u_BRDFLUT;
            uniform float u_Metallic; uniform float u_Roughness; uniform float u_AO;

            struct DirectionalLight { vec3 direction; vec3 color; float intensity; };
            struct PointLight { vec3 position; vec3 color; float intensity; float range; };
            struct SpotLight { vec3 position; vec3 direction; vec3 color; float intensity; float range; float innerCos; float outerCos; };
            uniform int u_HasDirectionalLight; uniform DirectionalLight u_DirectionalLight;
            uniform int u_PointLightCount; uniform PointLight u_PointLights[4];
            uniform int u_SpotLightCount; uniform SpotLight u_SpotLights[4];

            uniform sampler2D u_DirectionalShadowMap;
            uniform samplerCube u_PointShadowMap;
            uniform sampler2D u_SpotShadowMap;
            uniform int u_HasDirectionalShadow, u_HasPointShadow, u_HasSpotShadow;
            uniform mat4 u_DirectionalLightSpace, u_SpotLightSpace;
            uniform vec3 u_PointShadowPosition;
            uniform float u_PointShadowFar, u_DirectionalShadowBias, u_PointShadowBias, u_SpotShadowBias;

            const float PI=3.14159265359;
            float DistributionGGX(vec3 N,vec3 H,float r){float a=r*r,a2=a*a,NH=max(dot(N,H),0.0),NH2=NH*NH;float d=(NH2*(a2-1.0)+1.0);return a2/max(PI*d*d,0.000001);}
            float GeometrySchlickGGX(float NV,float r){float k=((r+1.0)*(r+1.0))/8.0;return NV/(NV*(1.0-k)+k);}
            float GeometrySmith(vec3 N,vec3 V,vec3 L,float r){return GeometrySchlickGGX(max(dot(N,V),0.0),r)*GeometrySchlickGGX(max(dot(N,L),0.0),r);}
            vec3 FresnelSchlick(float c,vec3 F0){return F0+(1.0-F0)*pow(clamp(1.0-c,0.0,1.0),5.0);}
            vec3 FresnelSchlickRoughness(float c,vec3 F0,float r){return F0+(max(vec3(1.0-r),F0)-F0)*pow(clamp(1.0-c,0.0,1.0),5.0);}

            vec3 SurfaceNormal(){
                vec3 N=normalize(v_Normal);
                if(u_UseNormalMap==0) return N;
                vec3 mapN=texture(u_NormalMap,v_TexCoord).xyz*2.0-1.0;
                mapN.xy*=u_NormalStrength;
                vec3 Q1=dFdx(v_WorldPosition), Q2=dFdy(v_WorldPosition);
                vec2 st1=dFdx(v_TexCoord), st2=dFdy(v_TexCoord);

                // Reconstruct the full cotangent frame from position/UV
                // derivatives. The previous B=-cross(N,T) assumed one fixed
                // UV handedness, which breaks on mirrored FBX/Mixamo UV
                // islands and can turn correctly textured areas almost black.
                float det=st1.x*st2.y-st1.y*st2.x;
                if(abs(det)<0.000001) return N;
                vec3 T=(Q1*st2.y-Q2*st1.y)/det;
                vec3 B=(-Q1*st2.x+Q2*st1.x)/det;
                T=normalize(T-N*dot(N,T));
                B=normalize(B-N*dot(N,B));
                return normalize(mat3(T,B,N)*mapN);
            }
            float Shadow2D(sampler2D map,mat4 lightSpace,float bias){
                vec4 lp=lightSpace*vec4(v_WorldPosition,1.0); vec3 p=lp.xyz/lp.w; p=p*0.5+0.5;
                if(p.z>1.0||p.x<0.0||p.x>1.0||p.y<0.0||p.y>1.0) return 0.0;
                float shadow=0.0; vec2 texel=1.0/vec2(textureSize(map,0));
                for(int x=-1;x<=1;x++)for(int y=-1;y<=1;y++) shadow += p.z-bias>texture(map,p.xy+vec2(x,y)*texel).r?1.0:0.0;
                return shadow/9.0;
            }
            float PointShadow(float bias){
                vec3 d=v_WorldPosition-u_PointShadowPosition; float current=length(d);
                float closest=texture(u_PointShadowMap,d).r*u_PointShadowFar;
                return current-bias>closest?1.0:0.0;
            }
            vec3 BRDF(vec3 albedo,vec3 N,vec3 V,vec3 L,vec3 radiance,float metallic,float roughness){
                vec3 H=normalize(V+L); vec3 F0=mix(vec3(0.04),albedo,metallic);
                vec3 F=FresnelSchlick(max(dot(H,V),0.0),F0);
                float NDF=DistributionGGX(N,H,roughness),G=GeometrySmith(N,V,L,roughness);
                vec3 spec=(NDF*G*F)/max(4.0*max(dot(N,V),0.0)*max(dot(N,L),0.0),0.001);
                vec3 kD=(vec3(1.0)-F)*(1.0-metallic); float NL=max(dot(N,L),0.0);
                return (kD*albedo/PI+spec)*radiance*NL;
            }
            vec3 RotateEnvironment(vec3 d){
                float c=cos(u_EnvironmentRotation),s=sin(u_EnvironmentRotation);
                return vec3(c*d.x+s*d.z,d.y,-s*d.x+c*d.z);
            }
            vec3 ProceduralEnvironment(vec3 d){
                d=RotateEnvironment(normalize(d));
                float h=clamp(d.y*0.5+0.5,0.0,1.0);
                vec3 horizon=vec3(0.62,0.72,0.86), zenith=vec3(0.08,0.22,0.48);
                vec3 sky=mix(horizon,zenith,pow(h,0.7));
                if(d.y<0.0) sky=mix(vec3(0.025,0.03,0.035),horizon,clamp(d.y+1.0,0.0,1.0)*0.18);
                return sky;
            }
            vec3 ACES(vec3 x){
                const float a=2.51,b=0.03,c=2.43,d=0.59,e=0.14;
                return clamp((x*(a*x+b))/(x*(c*x+d)+e),0.0,1.0);
            }
            void main(){
                vec4 base=u_Color; if(u_UseTexture==1) base*=texture(u_Texture,v_TexCoord);
                if(u_SurfaceMode==1 && base.a<u_AlphaCutoff) discard;
                vec3 albedo=max(base.rgb,vec3(0.0));
                float metallic=clamp(u_Metallic*(u_UseMetallicMap==1?texture(u_MetallicMap,v_TexCoord).r:1.0),0.0,1.0);
                float roughness=clamp(u_Roughness*(u_UseRoughnessMap==1?texture(u_RoughnessMap,v_TexCoord).r:1.0),0.04,1.0);
                float ao=clamp(u_AO*(u_UseAOMap==1?texture(u_AOMap,v_TexCoord).r:1.0),0.0,1.0);
                vec3 N=SurfaceNormal(), V=normalize(u_ViewPosition-v_WorldPosition);
                vec3 Lo=vec3(0.0);

                if(u_HasDirectionalLight==1){vec3 L=normalize(-u_DirectionalLight.direction);float sh=u_HasDirectionalShadow==1?Shadow2D(u_DirectionalShadowMap,u_DirectionalLightSpace,u_DirectionalShadowBias):0.0;Lo+=BRDF(albedo,N,V,L,u_DirectionalLight.color*u_DirectionalLight.intensity*(1.0-sh),metallic,roughness);}
                for(int i=0;i<u_PointLightCount;i++){vec3 delta=u_PointLights[i].position-v_WorldPosition;float dist=length(delta);vec3 L=delta/max(dist,0.0001);float a=clamp(1.0-dist/max(u_PointLights[i].range,0.001),0.0,1.0);a*=a;float sh=(i==0&&u_HasPointShadow==1)?PointShadow(u_PointShadowBias):0.0;Lo+=BRDF(albedo,N,V,L,u_PointLights[i].color*u_PointLights[i].intensity*a*(1.0-sh),metallic,roughness);}
                for(int i=0;i<u_SpotLightCount;i++){vec3 delta=u_SpotLights[i].position-v_WorldPosition;float dist=length(delta);vec3 L=delta/max(dist,0.0001);float theta=dot(normalize(-L),normalize(u_SpotLights[i].direction));float cone=smoothstep(u_SpotLights[i].outerCos,u_SpotLights[i].innerCos,theta);float a=clamp(1.0-dist/max(u_SpotLights[i].range,0.001),0.0,1.0);a*=a;float sh=(i==0&&u_HasSpotShadow==1)?Shadow2D(u_SpotShadowMap,u_SpotLightSpace,u_SpotShadowBias):0.0;Lo+=BRDF(albedo,N,V,L,u_SpotLights[i].color*u_SpotLights[i].intensity*a*cone*(1.0-sh),metallic,roughness);}

                // Procedural image-based lighting: diffuse sky irradiance + roughness-aware specular environment.
                vec3 F0=mix(vec3(0.04),albedo,metallic);
                float NV=max(dot(N,V),0.0);
                vec3 F=FresnelSchlickRoughness(NV,F0,roughness);
                vec3 kD=(vec3(1.0)-F)*(1.0-metallic);
                vec3 R=RotateEnvironment(reflect(-V,N));
                vec3 diffuseIBL;
                vec3 specIBL;
                if(u_HasHDRI==1){
                    vec3 irradiance=texture(u_IrradianceMap,RotateEnvironment(N)).rgb;
                    diffuseIBL=irradiance*albedo;
                    const float MAX_REFLECTION_LOD=4.0;
                    vec3 prefiltered=textureLod(
                        u_PrefilterMap,R,roughness*MAX_REFLECTION_LOD).rgb;
                    vec2 brdf=texture(u_BRDFLUT,vec2(NV,roughness)).rg;
                    specIBL=prefiltered*(F*brdf.x+brdf.y);
                }else{
                    diffuseIBL=ProceduralEnvironment(N)*albedo;
                    vec3 specEnv=ProceduralEnvironment(
                        normalize(mix(reflect(-V,N),N,roughness*roughness)));
                    specIBL=specEnv*F*(1.0-roughness*0.45);
                }
                vec3 environment =
                    (kD*diffuseIBL*u_DiffuseIBLStrength+
                     specIBL*u_SpecularIBLStrength)*
                    u_EnvironmentIntensity;
                if(u_UseIBL==0) environment=vec3(0.0);
                vec3 ambient=(environment+u_AmbientColor*albedo*0.25)*ao;

                vec3 emissive=u_EmissiveColor*u_EmissiveStrength;
                if(u_UseEmissiveMap==1) emissive*=texture(u_EmissiveMap,v_TexCoord).rgb;

                // Keep scene output linear/HDR. Tone mapping, bloom, AO and
                // anti-aliasing are handled by the framebuffer post-process chain.
                vec3 color=ambient+Lo+emissive;
                o_Color=vec4(color,base.a);
            }
        )";
}
