/***************************************************************************/
/* class_board_design_settings.cpp - EDA_BoardDesignSettings class functions */
/***************************************************************************/
#include "fctsys.h"
#include "common.h"

#include "pcbnew.h"
#include "class_board_design_settings.h"


/*****************************************************/
EDA_BoardDesignSettings::EDA_BoardDesignSettings()
/*****************************************************/

// Default values for designing boards
{
    int ii;

    static const int default_layer_color[32] =
    {
        GREEN,     LIGHTGRAY, LIGHTGRAY, LIGHTGRAY,
        LIGHTGRAY, LIGHTGRAY, LIGHTGRAY, LIGHTGRAY,
        LIGHTGRAY, LIGHTGRAY, LIGHTGRAY, LIGHTGRAY,
        LIGHTGRAY, LIGHTGRAY, LIGHTGRAY, RED,
        LIGHTGRAY, LIGHTGRAY,
        LIGHTGRAY, LIGHTGRAY,
        MAGENTA,   CYAN,
        LIGHTGRAY, LIGHTGRAY,
        LIGHTGRAY,
        LIGHTGRAY,
        LIGHTGRAY, LIGHTGRAY,
        LIGHTGRAY,
        LIGHTGRAY,
        LIGHTGRAY,
        LIGHTGRAY
    };

    m_EnabledLayers     = ALL_LAYERS;               // All layers enabled at first.
                                                    // SetCopperLayerCount() will adjust thist.
    m_VisibleLayers     = 0xffffffff;               // IMB: All layers visible at first. TODO: Use a macro for the initial value.
    m_VisibleElements   = 0x00000fff;               // IMB: All elements visible at first. TODO: Use a macro for the initial value.

    SetCopperLayerCount( 2 );                    // Default design is a double sided board
    m_CurrentViaType        = VIA_THROUGH;          // via type (VIA_BLIND_BURIED, VIA_THROUGH VIA_MICROVIA)
    m_UseConnectedTrackWidth = false;               // if true, when creating a new track starting on an existing track, use this track width
    m_MicroViasAllowed      = false;                // true to allow micro vias
    m_DrawSegmentWidth      = 100;                  // current graphic line width (not EDGE layer)
    m_EdgeSegmentWidth      = 100;                  // current graphic line width (EDGE layer only)
    m_PcbTextWidth          = 100;                  // current Pcb (not module) Text width
    m_PcbTextSize           = wxSize( 500, 500 );   // current Pcb (not module) Text size
    m_TrackMinWidth         = 80;                   // track min value for width ((min copper size value
    m_ViasMinSize           = 350;                  // vias (not micro vias) min diameter
    m_ViasMinDrill          = 200;                  // vias (not micro vias) min drill diameter
    m_MicroViasMinSize      = 200;                  // micro vias (not vias) min diameter
    m_MicroViasMinDrill     = 50;                   // micro vias (not vias) min drill diameter
    m_MaskMargin            = 150;                  // Solder mask margin

    /* Color options for screen display of the Printed Board: */
    for( ii = 0; ii < 32; ii++ )
        m_LayerColor[ii] = default_layer_color[ii];

    // Layer colors (tracks and graphic items)
    m_ViaColor[VIA_NOT_DEFINED]     = DARKGRAY;
    m_ViaColor[VIA_MICROVIA]        = CYAN;
    m_ViaColor[VIA_BLIND_BURIED]    = BROWN;
    m_ViaColor[VIA_THROUGH]         = WHITE;

    m_RatsnestColor = WHITE;                // Ratsnest color
}


// see pcbstruct.h
int EDA_BoardDesignSettings::GetVisibleLayers() const
{
    return m_VisibleLayers;
}

void EDA_BoardDesignSettings::SetVisibleLayers( int aMask )
{
    m_VisibleLayers = aMask & m_EnabledLayers & ALL_LAYERS;
}

void EDA_BoardDesignSettings::SetLayerVisibility( int aLayerIndex, bool aNewState )
{
    // Altough Pcbnew uses only 29, Gerbview uses all 32 layers
    if( aLayerIndex < 0 || aLayerIndex >= 32 )
        return;
    if( aNewState && IsLayerEnabled( aLayerIndex ))
        m_VisibleLayers |= 1 << aLayerIndex;
    else
        m_VisibleLayers &= ~( 1 << aLayerIndex );
}

void EDA_BoardDesignSettings::SetElementVisibility( int aElementCategory, bool aNewState )
{
    if( aElementCategory < 0 || aElementCategory > PAD_CMP_VISIBLE )
        return;
    if( aNewState )
        m_VisibleElements |= 1 << aElementCategory;
    else
        m_VisibleElements &= ~( 1 << aElementCategory );
}
/**
 * Function SetCopperLayerCount
 * do what its name says...
 * @param aNewLayerCount = The new number of enabled copper layers
 */
void EDA_BoardDesignSettings::SetCopperLayerCount( int aNewLayerCount )
{
    m_CopperLayerCount = aNewLayerCount;
    // ensure consistency with the m_EnabledLayers member
    m_EnabledLayers &= ~ALL_CU_LAYERS;
    m_EnabledLayers |= CUIVRE_LAYER;
    if ( m_CopperLayerCount > 1 )
        m_EnabledLayers |= CMP_LAYER;
    for( int ii = 1; ii < aNewLayerCount-1; ii++ )
        m_EnabledLayers |= 1 << ii;
}