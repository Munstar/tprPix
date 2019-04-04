/*
 * ====================== Chunk.cpp =======================
 *                          -- tpr --
 *                                        CREATE -- 2018.12.09
 *                                        MODIFY -- 
 * ----------------------------------------------------------
 *    map chunk 一个地图区域。 左下坐标系
 *    ----------
 *    
 * ----------------------------
 */
#include "Chunk.h"

//-------------------- C --------------------//
#include <cassert>
#include <math.h>

//-------------------- CPP --------------------//
#include <map>

//-------------------- Engine --------------------//
#include "ViewingBox.h"
#include "srcs_engine.h"
#include "MapEnt.h"
#include "EcoSysInMap.h"
#include "random.h"
#include "occupyWeight.h"
#include "EcoSys.h"
#include "Altitude.h"
#include "Quad.h"
#include "FieldBorderSet.h"

#include "debug.h"


namespace{//-------- namespace: --------------//

    std::default_random_engine  randEngine; //-通用 随机数引擎实例
    inline std::uniform_int_distribution<int> uDistribution_regular(0,1000); // [0,1000]
    bool  is_rand_init {false}; //- tmp
    
    //--- 定值: chunk-mesh scale --
    glm::vec3  mesh_scaleVal {  (float)(ENTS_PER_CHUNK * PIXES_PER_MAPENT),
                                (float)(ENTS_PER_CHUNK * PIXES_PER_MAPENT),
                                1.0f };

    std::vector<fieldKey_t> fieldKeys {}; //- 8*8 fieldKeys


    //--- 临时 沙滩色，水色 ---
    RGBA color_sand  {210, 195, 142, 255};
    RGBA color_water { 97, 125, 142, 255 };

    RGBA color_deepWater { 32, 60, 77, 255 };

    RGBA color_water_lvl1 { 32, 80, 77, 255 }; //- 水下 -1 层叠加色
    RGBA color_water_lvl2 { 32, 75, 77, 255 };
    RGBA color_water_lvl3 { 32, 70, 77, 255 };
    RGBA color_water_lvl4 { 32, 65, 77, 255 };
    RGBA color_water_lvl5 { 32, 60, 77, 255 };

    RGBA color_multi { 90, 110, 105, 255 }; //- 测试 正片叠底 用



    class FieldData{
    public:
        explicit FieldData( MapField *_fieldPtr, QuadType _quadType ){
            this->fieldPtr = _fieldPtr;
            this->ecoInMapPtr = esrc::get_ecoSysInMapPtr( this->fieldPtr->ecoSysInMapKey );
            this->ecoPtr = esrc::get_ecoSysPtr( this->ecoInMapPtr->ecoSysType );
            this->quadContainerPtr = (FieldBorderSet::quadContainer_t*)&get_fieldBorderSet( this->fieldPtr->fieldBorderSetId, _quadType );
        }
        //====== vals ======//
        MapField     *fieldPtr    {};
        EcoSysInMap  *ecoInMapPtr {};
        EcoSys       *ecoPtr      {};
        FieldBorderSet::quadContainer_t  *quadContainerPtr; //-- fieldBorderSet 子扇区容器 --
        //...
    };


    class PixData{
    public:
        inline void init( const IntVec2 &_ppos ){
            this->ppos = _ppos;
        }
        //====== vals ======//
        size_t     pixIdx_in_chunk    {};
        size_t     pixIdx_in_field    {};
        IntVec2    ppos      {};
        RGBA      *texPixPtr {nullptr};
        FieldData *fieldDataPtr {nullptr}; 
        Altitude   alti {};
    };



    std::map<occupyWeight_t,FieldData> nearFour_fieldDatas {}; //- 一个 field 周边4个 field 数据
                                    // 按照 ecoSysInMap.occupyWeight 倒叙排列（值大的在前面）

    class FieldInfo{
    public:
        IntVec2   mposOff;
        QuadType  quad; //- 注意，这个值是反的，从 fieldBorderSet 角度去看 得到的 quad
    };

    //- 周边4个field 指示数据 --
    std::vector<FieldInfo> nearFour_fieldInfos {
        FieldInfo{ IntVec2{ 0, 0 },                           QuadType::Right_Top },
        FieldInfo{ IntVec2{ ENTS_PER_FIELD, 0 },              QuadType::Left_Top  },
        FieldInfo{ IntVec2{ 0, ENTS_PER_FIELD },              QuadType::Right_Bottom  },
        FieldInfo{ IntVec2{ ENTS_PER_FIELD, ENTS_PER_FIELD }, QuadType::Left_Bottom  }
    };

    bool  is_baseUniforms_transmited {false}; //- pixGpgpu 的几个 静态uniform值 是否被传输
                                        // 这些值是固定的，每次游戏只需传入一次...


    //===== funcs =====//
    MapField *colloect_and_creat_nearFour_fieldDatas( fieldKey_t _fieldKey );

    


}//------------- namespace: end --------------//


/* ===========================================================
 *                        init
 * -----------------------------------------------------------
 */
void Chunk::init(){
    
    if( is_rand_init == false ){
        is_rand_init = true;
        randEngine.seed( get_new_seed() );
    }
    

    //--- mesh.scale ---
    mesh.set_scale(mesh_scaleVal);

    //---
    this->init_memMapEnts();

    //--- 填充 mapTex buf
    this->mapTex.resize_texBuf();
}


/* ===========================================================
 *                  refresh_translate_auto
 * -----------------------------------------------------------
 */
void Chunk::refresh_translate_auto(){
    const IntVec2 &ppos = mcpos.get_ppos();
    mesh.set_translate(glm::vec3{   (float)ppos.x,
                                    (float)ppos.y,
                                    esrc::camera.get_zFar() + ViewingBox::chunks_zOff //-- MUST --
                                    });
}


/* ===========================================================
 *                     init_memMapEnts
 * -----------------------------------------------------------
 * -- 向 memMapEnts 填入每个mapent，并设置它们的 mcpos
 * --- 除此之外，这些 mapent 数据都是空的
 */
void Chunk::init_memMapEnts(){
    if( this->is_memMapEnts_set ){
        return;
    }
    MemMapEnt mapEnt {};
    for( int h=0; h<ENTS_PER_CHUNK; h++ ){
        for( int w=0; w<ENTS_PER_CHUNK; w++ ){
            mapEnt.mcpos = mcpos + MapCoord{ w, h };
            this->memMapEnts.push_back( mapEnt ); //-copy
        }
    }
    this->is_memMapEnts_set = true;
}


/* ===========================================================
 *               get_mapEntIdx_in_chunk
 * -----------------------------------------------------------
 * -- 传入任意 mpos，获得其在 本chunk 中的 idx（访问 vector 用）
 */
size_t Chunk::get_mapEntIdx_in_chunk( const IntVec2 &_anyMPos ){
    IntVec2 mposOff = _anyMPos - this->mcpos.get_mpos();
    int w = mposOff.x;
    int h = mposOff.y;
        assert( (w>=0) && (w<ENTS_PER_CHUNK) &&
                (h>=0) && (h<ENTS_PER_CHUNK) ); //- tmp
    return (size_t)(h*ENTS_PER_CHUNK + w);
}


/* ===========================================================
 *               get_pixIdx_in_chunk
 * -----------------------------------------------------------
 * -- 传入任意 ppos 绝对值，获得 此pix 在 本chunk 中的 idx（访问 mapTex 用）
 */
size_t Chunk::get_pixIdx_in_chunk( const IntVec2 &_anyPPos ){
    IntVec2 pposOff = _anyPPos - this->mcpos.get_ppos();
    int w = pposOff.x;
    int h = pposOff.y;
        assert( (w>=0) && (w<PIXES_PER_CHUNK) &&
                (h>=0) && (h<PIXES_PER_CHUNK) ); //- tmp
    return (size_t)( h*PIXES_PER_CHUNK + w );
}



/* ===========================================================
 *           assign_ents_and_pixes_to_field
 * -----------------------------------------------------------
 */
void Chunk::assign_ents_and_pixes_to_field(){
    if( this->is_assign_ents_and_pixes_to_field_done ){
        return;
    }

    MapField  *tmpFieldPtr;
    RGBA      *texBufHeadPtr; //- mapTex
    RGBA       color;

    u8_t r;
    u8_t g;
    u8_t b;

    IntVec2    tmpEntMPos;
    IntVec2    pposOff;   //- 通用偏移向量

    float   off;
    float   freq = 1.7;

    PixData   pixData;//- each pix

    int    count;

    int    randVal;

    float   waterStep = 0.06; //- 用于 water 颜色混合

    texBufHeadPtr = this->mapTex.get_texBufHeadPtr();


    //------------------------//
    // 委托 GPGPU 计算 pix数据
    //------------------------//
    esrc::pixGpgpu.bind(); //--- MUST !!! ---

    IntVec2 chunkCPos = chunkMPos_2_chunkCPos( this->mcpos.get_mpos() );
    glUniform2f(esrc::pixGpgpu.get_uniform_location("chunkCFPos"), 
                static_cast<float>(chunkCPos.x), 
                static_cast<float>(chunkCPos.y) ); //- 2-float

    //-- 每个游戏存档的这组值 其实是固定的，游戏运行期间，只需传输一次 --
    if( is_baseUniforms_transmited == false ){
        is_baseUniforms_transmited = true;
        glUniform2f(esrc::pixGpgpu.get_uniform_location("altiSeed_pposOffSeaLvl"), 
                    esrc::gameSeed.altiSeed_pposOffSeaLvl.x,
                    esrc::gameSeed.altiSeed_pposOffSeaLvl.y ); //- 2-float
                    
        glUniform2f(esrc::pixGpgpu.get_uniform_location("altiSeed_pposOffBig"), 
                    esrc::gameSeed.altiSeed_pposOffBig.x,
                    esrc::gameSeed.altiSeed_pposOffBig.y ); //- 2-float
        
        glUniform2f(esrc::pixGpgpu.get_uniform_location("altiSeed_pposOffMid"), 
                    esrc::gameSeed.altiSeed_pposOffMid.x,
                    esrc::gameSeed.altiSeed_pposOffMid.y ); //- 2-float

        glUniform2f(esrc::pixGpgpu.get_uniform_location("altiSeed_pposOffSml"), 
                    esrc::gameSeed.altiSeed_pposOffSml.x,
                    esrc::gameSeed.altiSeed_pposOffSml.y ); //- 2-float
    }
                    

    esrc::pixGpgpu.let_gpgpuFBO_work();
    const std::vector<FRGB> &gpgpuDatas = esrc::pixGpgpu.get_texFBuf();
    esrc::pixGpgpu.release(); //--- MUST !!! ---
    

    //------------------------//
    //   reset fieldPtrs
    //------------------------//
    this->reset_fieldKeys();

    for( const auto &fieldKey : fieldKeys ){ //- each field key

        //-- 收集 目标field 周边4个 field 实例指针  --
        // 如果相关 field 不存在，就地创建之
        tmpFieldPtr = colloect_and_creat_nearFour_fieldDatas( fieldKey );


        for( int eh=0; eh<ENTS_PER_FIELD; eh++ ){
            for( int ew=0; ew<ENTS_PER_FIELD; ew++ ){ //- each ent in field

                tmpEntMPos = tmpFieldPtr->get_mpos() + IntVec2{ ew, eh };
                //...

                for( int ph=0; ph<PIXES_PER_MAPENT; ph++ ){
                    for( int pw=0; pw<PIXES_PER_MAPENT; pw++ ){ //------ each pix in mapent ------

                        pixData.init( mpos_2_ppos(tmpEntMPos) + IntVec2{pw,ph} );
                        pposOff = pixData.ppos - this->mcpos.get_ppos();
                        pixData.pixIdx_in_chunk = pposOff.y * PIXES_PER_CHUNK + pposOff.x;
                        pixData.texPixPtr = texBufHeadPtr + pixData.pixIdx_in_chunk;

                        pposOff = pixData.ppos - tmpFieldPtr->get_ppos();
                        pixData.pixIdx_in_field = pposOff.y * PIXES_PER_FIELD + pposOff.x;


                        //--------------------------------//
                        // 确定 pix 属于 周边4个field 中的哪一个
                        //--------------------------------//
                        count = 0;
                        for( auto &fieldPair : nearFour_fieldDatas ){ //--- 周边4个 field 信息
                            count++;
                            const FieldData &fieldDataRef = fieldPair.second;
                            if( count != nearFour_fieldDatas.size() ){   //- 前3个 field
                                if( fieldDataRef.quadContainerPtr->at(pixData.pixIdx_in_field) == 1 ){
                                    pixData.fieldDataPtr = (FieldData*)&fieldDataRef;
                                    break;
                                }
                            }else{     //- 第4个 field
                                pixData.fieldDataPtr = (FieldData*)&fieldDataRef;
                            }
                        } //--- 周边4个 field 信息 end ---


                        //--------------------------------//
                        //  计算 本pix  alti
                        //--------------------------------//
                        pixData.alti.set( gpgpuDatas.at(pixData.pixIdx_in_chunk).r );


                        //--------------------------------//
                        // 数据收集完毕，将部分数据 传递给 ent
                        //--------------------------------//
                        if( (ph==HALF_PIXES_PER_MAPENT) && (pw==HALF_PIXES_PER_MAPENT) ){//- ent 中点 pix

                            //...
                        }

                        //--------------------------------//
                        //    正式给 pix 上色
                        //--------------------------------//

                        color = pixData.fieldDataPtr->ecoPtr->color_low;
                        color.r = (u8_t)(color.r + pixData.fieldDataPtr->fieldPtr->lColorOff_r);
                        color.g = (u8_t)(color.g + pixData.fieldDataPtr->fieldPtr->lColorOff_g);
                        color.b = (u8_t)(color.b + pixData.fieldDataPtr->fieldPtr->lColorOff_b);

                        if( pixData.alti.lvl < 0 ){
                            color.a = 0;
                        }else{
                            color.a = 255;
                        }
                        

                        
                        *pixData.texPixPtr = RGBA{ color.r, color.g, color.b, color.a };


                    }
                } //- each pix in mapent end ---

            }
        } //- each ent in field end --


    } //-- each field key end --

    //---------------------------//
    //   正式用 texture 生成 name
    //---------------------------//
    this->mapTex.creat_texName();

    this->is_assign_ents_and_pixes_to_field_done = true;
}



/* ===========================================================
 *                   reset_fieldKeys
 * -----------------------------------------------------------
 * -- fieldKeys 是个 局部公用容器，每次使用前，都要重装填
 */
void Chunk::reset_fieldKeys(){

    IntVec2    tmpFieldMpos;
    fieldKeys.clear();
    for( int h=0; h<FIELDS_PER_CHUNK; h++ ){
        for( int w=0; w<FIELDS_PER_CHUNK; w++ ){ //- each field

            tmpFieldMpos = this->get_mpos() + IntVec2{  w*ENTS_PER_FIELD,
                                                        h*ENTS_PER_FIELD };
            fieldKeys.push_back( fieldMPos_2_fieldKey(tmpFieldMpos) );
        }
    }
}


namespace{//-------- namespace: --------------//



/* ===========================================================
 *              colloect_and_creat_nearFour_fieldDatas
 * -----------------------------------------------------------
 * -- 收集 目标field 周边4个 field 数据
 *    如果相关 field 不存在，就地创建之
 * -- 返回 目标 field 指针
 */
MapField *colloect_and_creat_nearFour_fieldDatas( fieldKey_t _fieldKey ){

    MapField     *targetFieldPtr = esrc::find_or_insert_the_field( _fieldKey );
    MapField     *tmpFieldPtr;
    IntVec2       tmpFieldMPos;
    int           count = 0;
    //---
    nearFour_fieldDatas.clear();
    for( const auto &fieldInfo : nearFour_fieldInfos ){

        if( count == 0 ){
            tmpFieldPtr = targetFieldPtr;
        }else{
            tmpFieldMPos = targetFieldPtr->get_mpos() + fieldInfo.mposOff;
            tmpFieldPtr = esrc::find_or_insert_the_field( fieldMPos_2_fieldKey(tmpFieldMPos) );
        }

        nearFour_fieldDatas.insert({ -(tmpFieldPtr->occupyWeight), FieldData{tmpFieldPtr,fieldInfo.quad} }); //- copy
                        //- 通过负数，来实现 倒叙排列，occupyWeight 值大的排前面
        count++;
    }

    return targetFieldPtr;
}






}//------------- namespace: end --------------//
