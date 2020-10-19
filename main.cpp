/*
* Hex-Rays OLLVM Deobfuscator and MicroCode Explorer
* Original by Rolf Folles
* Ported from https://github.com/RolfRolles/HexRaysDeob
*
* Pedro Vilaça - reverser@put.as - https://reverse.put.as
*
* Implements all options in a menu item on pseudo code view
*
* Allows to runtime enable/disable the deobfuscator
* After enable/disable press F5 again in the pseuco code view to refresh
*
* Reference: https://www.hex-rays.com/blog/hex-rays-microcode-api-vs-obfuscating-compiler/
*
* Alternative microcode explorer with different features is Lucid
* https://github.com/gaasedelen/lucid
* https://blog.ret2.io/2020/09/11/lucid-hexrays-microcode-explorer/
*
* This uses the new C++ plugin API so it's only >= IDA 7.5 compatible
*
* Based on IDA SDK ht_view sample plugin
*
*/

#include <set>
#define USE_DANGEROUS_FUNCTIONS 
#include <hexrays.hpp>
#include <idp.hpp>
#include "HexRaysUtil.hpp"
#include "MicrocodeExplorer.hpp"
#include "PatternDeobfuscate.hpp"
#include "AllocaFixer.hpp"
#include "Unflattener.hpp"
#include "Config.hpp"

#if IDA_SDK_VERSION < 750
#error This code requires IDA SDK 7.5 or higher
#endif

#define VERSION "1.0"

#define ENABLE_ACTION_NAME	"hdo:Enable"
#define DISABLE_ACTION_NAME	"hdo:Disable"
#define MICROCODE_ACTION_NAME "hdo:MicroCode"
#define ALLOCPROBE_ACTION_NAME "hdo:FixAlloca"

extern plugin_t PLUGIN;

/// Pointer to Hex-Rays decompiler dispatcher.
/// This variable must be instantiated by the plugin. It is initialized by init_hexrays_plugin().
hexdsp_t *hexdsp = NULL;

ObfCompilerOptimizer hook;
CFUnflattener cfu;

/*
 * the action handlers responsible for doing something
 * when we select the menu options
 * we could merge in a single handler and implement different
 * logic to handle options inside?
 */
struct deobfuscator_ctx_t;
struct enable_ah_t : public action_handler_t
{
	deobfuscator_ctx_t &p;
	enable_ah_t(deobfuscator_ctx_t &c) : p(c) {}
    virtual int idaapi activate(action_activation_ctx_t *ctx) override;
    virtual action_state_t idaapi update(action_update_ctx_t *ctx) override;
};

struct disable_ah_t : public action_handler_t
{
	deobfuscator_ctx_t &p;
	disable_ah_t(deobfuscator_ctx_t &c) : p(c) {}
    virtual int idaapi activate(action_activation_ctx_t *ctx) override;
    virtual action_state_t idaapi update(action_update_ctx_t *ctx) override;
};

struct allocprob_ah_t : public action_handler_t
{
    virtual int idaapi activate(action_activation_ctx_t *ctx) override
    {
		FixCallsToAllocaProbe();
		return true;
    }
    virtual action_state_t idaapi update(action_update_ctx_t *ctx) override
    {
    	return AST_ENABLE_ALWAYS;
    }
};

struct microcode_ah_t : public action_handler_t
{
    // the menu item was selected so we can launch the explorer itself
    virtual int idaapi activate(action_activation_ctx_t *) override
    {
        ShowMicrocodeExplorer();
        return true;
    }

    virtual action_state_t idaapi update(action_update_ctx_t *) override
    {
        return AST_ENABLE_ALWAYS;
    }
};

struct deobfuscator_ctx_t : public plugmod_t
{
    bool hooked = false;
    int enabled = 0;

    enable_ah_t enable_ah = enable_ah_t(*this);
    const action_desc_t enable_action = ACTION_DESC_LITERAL_PLUGMOD(
        ENABLE_ACTION_NAME,
        "Enable OLLVM deobfuscator",
        &enable_ah,
        this,
        NULL,
        "Enable OLLVM deobfuscator",
        -1);

    disable_ah_t disable_ah = disable_ah_t(*this);
    const action_desc_t disable_action = ACTION_DESC_LITERAL_PLUGMOD(
        DISABLE_ACTION_NAME,
        "Disable OLLVM deobfuscator",
        &disable_ah,
        this,
        NULL,
        "Disable OLLVM deobfuscator",
        -1);

    microcode_ah_t ah;
    const action_desc_t microcode_action = ACTION_DESC_LITERAL_PLUGMOD(
        MICROCODE_ACTION_NAME,
        "Start MicroCode Explorer",
        &ah,
        this,
        NULL,
        "Open the MicroCode Explorer",
        -1);

    allocprob_ah_t allocprobe_ah;
    const action_desc_t fixallocaprobe_action = ACTION_DESC_LITERAL_PLUGMOD(
        ALLOCPROBE_ACTION_NAME,
        "Fix Calls To AllocaProbe",
        &allocprobe_ah,
        this,
        NULL,
        "Fix Calls To AllocaProbe",
        -1);

    deobfuscator_ctx_t();
    ~deobfuscator_ctx_t();
    
    virtual bool idaapi run(size_t arg) override;
};

//---------------------------------------------------------------------------
int idaapi enable_ah_t::activate(action_activation_ctx_t *ctx)
{
   	// Install our block and instruction optimization classes.
   	// XXX: probably just set a variable that is checked on the classes
   	// entrypoint to dynamically enable/disable them
	install_optinsn_handler(&hook);
	install_optblock_handler(&cfu);
	p.enabled = 1;		
	return true;
}

action_state_t idaapi enable_ah_t::update(action_update_ctx_t *ctx)
{
	// this is called all the time so discard if we are at the wrong widget
	// XXX: really necessary?
	if (ctx->widget_type != BWN_PSEUDOCODE) {
		return AST_ENABLE;
	}
	switch (p.enabled) {
		// if deobfuscator is disabled then activate the enable menu item
		case 0:
			return AST_ENABLE;
		// if deobfuscator is enabled then deactivate the enable menu item
		case 1:
			return AST_DISABLE;
		// shutup compiler
		default:
			return AST_ENABLE;
	}      
}

int idaapi disable_ah_t::activate(action_activation_ctx_t *ctx)
{
	remove_optinsn_handler(&hook);
	remove_optblock_handler(&cfu);
	p.enabled = 0;
	return true;
}

action_state_t idaapi disable_ah_t::update(action_update_ctx_t *ctx)
{
	// this is called all the time so discard if we are at the wrong widget
	if (ctx->widget_type != BWN_PSEUDOCODE) {
		return AST_ENABLE;
	}
	switch (p.enabled) {
		// if deobfuscator is disabled then deactivate the disable menu item
		case 0:
			return AST_DISABLE;
		// if deobfuscator is enabled then activate the disable menu item
		case 1:
			return AST_ENABLE;
		default:
			return AST_ENABLE;
	}
}

//---------------------------------------------------------------------------
// Callback for ui notifications
static ssize_t idaapi ui_callback(void *ud, int notification_code, va_list va)
{
    switch (notification_code)
    {
        // called when IDA is preparing a context menu for a view
        // Here dynamic context-depending user menu items can be added.
        case ui_finish_populating_widget_popup:
        {
            TWidget *view = va_arg(va, TWidget *);
            // BWN_PSEUDOCODE is for hex-rays output view
            // check include/kernwin.hpp for available window types
            if ( get_widget_type(view) == BWN_PSEUDOCODE )
            {
                TPopupMenu *p = va_arg(va, TPopupMenu *);
                // attach all our menu actions
                attach_action_to_popup(view, p, ENABLE_ACTION_NAME);
                attach_action_to_popup(view, p, DISABLE_ACTION_NAME);
                attach_action_to_popup(view, p, MICROCODE_ACTION_NAME);
                attach_action_to_popup(view, p, ALLOCPROBE_ACTION_NAME);
            }
            break;
        }
    }
    return 0;
}

//--------------------------------------------------------------------------
plugmod_t * idaapi init(void)
{
    if (is_idaq() == false) {
        return nullptr;
    }
	if (!init_hexrays_plugin()) {
        msg("--------------------------------------------\n");
        msg("OLLVM Deobfuscator %s\n", VERSION);
        msg("No Hex-Rays decompiler found, plugin not loaded.\n");
        msg("--------------------------------------------\n");
        return nullptr;
    }

    const char *hxver = get_hexrays_version();
    msg("--------------------------------------------\n");
    msg("OLLVM Deobfuscator %s\n", VERSION);
    msg("Hex-Rays version %s has been detected, plugin is ready to use\n", hxver);
    msg("--------------------------------------------\n");

	return new deobfuscator_ctx_t;
}

//--------------------------------------------------------------------------
deobfuscator_ctx_t::deobfuscator_ctx_t() 
{
	// we need to register each action and then attach to a menu item in the callback
	// the moment we register the items are available via shortcut if it exists
    register_action(enable_action);
    register_action(disable_action);
    register_action(microcode_action);
    register_action(fixallocaprobe_action);
    /* set callback for view notifications */
    if (!hooked) {
        hook_to_notification_point(HT_UI, ui_callback, this);
        hooked = true;
    }
}

deobfuscator_ctx_t::~deobfuscator_ctx_t()
{
	if (hooked) {
    	unhook_from_notification_point(HT_UI, ui_callback, this);
    }
    if (hexdsp != NULL) {
		// Uninstall our block and instruction optimization classes.
		if (enabled) {
			remove_optinsn_handler(&hook);
			remove_optblock_handler(&cfu);
		}
		// I couldn't figure out why, but my plugin would segfault if it tried
		// to free mop_t pointers that it had allocated. Maybe hexdsp had been
		// set to NULL at that point, so the calls to delete crashed? Anyway,
		// cleaning up before we unload solved the issues.
		cfu.Clear(true);
        term_hexrays_plugin();
    }
    msg("---------------------------\n");
    msg("Unloaded OLLVM Deobfuscator\n");
    msg("---------------------------\n");
}

//--------------------------------------------------------------------------
bool idaapi deobfuscator_ctx_t::run(size_t)
{
	return true;
}

//--------------------------------------------------------------------------
static const char comment[] = "OLLVM deobfuscator";

//--------------------------------------------------------------------------
//
//      PLUGIN DESCRIPTION BLOCK
//
//--------------------------------------------------------------------------
plugin_t PLUGIN =
{
	IDP_INTERFACE_VERSION,
	PLUGIN_UNL | PLUGIN_MULTI, 	// plugin flags
	init,                 	  	// initialize
	nullptr,                 	// terminate. this pointer may be NULL.
	nullptr,                  	// invoke plugin
	comment,              		// long comment about the plugin
						  		// it could appear in the status line
						  		// or as a hint
	"",                   		// multiline help about the plugin
	"OLLVM deobfuscator", 		// the preferred short name of the plugin
	""                    		// the preferred hotkey to run the plugin
};
