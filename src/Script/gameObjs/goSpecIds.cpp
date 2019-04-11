/*
 * ========================= goSpecIds.cpp ==========================
 *                          -- tpr --
 *                                        CREATE -- 2018.12.23
 *                                        MODIFY -- 
 * ----------------------------------------------------------
 *   将 具象go species 注册到 
 *    ssrc::go_specId_names 
 *    ssrc::go_name_specIds 中去
 * ----------------------------
 */
//-------------------- CPP --------------------//
#include <string>


//-------------------- Engine --------------------//
#include "GameObj.h" //- goSpecId_t

//-------------------- Script --------------------//
#include "Script/resource/srcs_script.h" 

#include "Script/gameObjs/Norman.h" 
#include "Script/gameObjs/BigMan.h" 
#include "Script/gameObjs/OakTree.h" 
#include "Script/gameObjs/mapSurfaces/Lichen.h"
#include "Script/gameObjs/bush/Wheat.h"


using std::string;

//#include "debug.h" //- tmp


namespace{//------------------- namespace ---------------------
    void goSpec_alloc( goSpecId_t _id, const string &_name );
}//------------------------- namespace: end -------------------



/* ===========================================================
 *                   onGoSpecIds_SignUp   
 * -----------------------------------------------------------
 * 
 */
void onGoSpecIds_SignUp(){

    //goSpecIds.insert(0); //--- 0号id 保留。
    //==================================//
    goSpec_alloc( 1, "norman" );  gameObjs::Norman::specId = 1;
    goSpec_alloc( 2, "bigMan" );  gameObjs::BigMan::specId = 2;

    //----- tree -----
    goSpec_alloc( 1001, "oakTree" ); gameObjs::OakTree::specId = 1001; //- 橡树
    //goSpec_alloc( 1000, "Tree_Pine" ); //- 松树


    //----- bush -----
    goSpec_alloc( 2001, "wheat" ); gameObjs::Wheat::specId = 2001; //- 小麦

    //----- mapSurface -----
    goSpec_alloc( 3001, "lichen" ); gameObjs::Lichen::specId = 3001; //- 地衣


    //cout << "--- onGoSpecIds_SignUp(). DONE. ---" << endl; 
}



namespace{//------------------- namespace ---------------------



/* ===========================================================
 *                   goSpec_alloc      
 * -----------------------------------------------------------
 * -- 拼装 正反表
 */
void goSpec_alloc( goSpecId_t _id, const string &_name ){

    ssrc::go_specId_names.insert({ _id, _name });
    ssrc::go_name_specIds.insert({ _name, _id });
}

}//------------------------- namespace: end -------------------


void onStart_test(){
    //cout << "--- onStart_test(). DONE. ---" << endl;
}



