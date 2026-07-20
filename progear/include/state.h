/*
 * This file is part of ProGearSDK.
 * Copyright (c) 2024-2025 ProGearSDK contributors
 * SPDX-License-Identifier: MIT
 */

/**
 * @file state.h
 * @brief Game state / screen switching.
 *
 * An NGState is a self-contained screen or mode of the game (title
 * screen, a level, a game-over screen, ...). Exactly one state runs at
 * a time. Each state provides an init/update/cleanup triple; the update
 * function returns the id of the next state to switch to, or
 * NG_STATE_ID_NONE to keep running.
 *
 * This replaces the hand-written "big switch statement in main()" that
 * every game would otherwise have to write to move between screens: the
 * engine drives the current state and handles the init/cleanup/arena-reset
 * sequence on transitions.
 *
 * @section stateusage Usage
 * 1. Write each screen as a set of `Init`/`Update`/`Cleanup` functions
 * 2. Register each one with NGStateRegister()
 * 3. Call NGStateStart() once at startup with the first state's id
 * 4. Call NGStateUpdate() once per frame (NGEngineFrameEnd() does this)
 */

#ifndef NG_STATE_H
#define NG_STATE_H

#include <ng_types.h>

/**
 * @defgroup state Game State System
 * @ingroup sdk
 * @brief Switching between self-contained game screens/modes.
 * @{
 */

/** @name Constants */
/** @{ */

/** No state / stay on the current state (returned by an update function). */
#define NG_STATE_ID_NONE 0

/** Maximum number of distinct states that can be registered. */
#define NG_STATE_MAX 16
/** @} */

/** @name Callback Types */
/** @{ */

/** Called once when a state becomes current. */
typedef void (*NGStateInitFn)(void);

/**
 * Called once per frame while a state is current.
 * @return NG_STATE_ID_NONE to keep running, or the id of the state to
 *         switch to next.
 */
typedef u8 (*NGStateUpdateFn)(void);

/** Called once when a state stops being current, before the next state's
 *  init function runs. Can be NULL if the state has nothing to tear down. */
typedef void (*NGStateCleanupFn)(void);
/** @} */

/** @name Registration */
/** @{ */

/**
 * Register a state.
 *
 * The name is what a scene-selection menu shows, so a game gets one for free
 * from the registration it already has to do (see NGSceneMenu).
 *
 * @param id State id (1..NG_STATE_MAX-1; 0 is reserved for NG_STATE_ID_NONE)
 * @param name Display name, or NULL if the state is never listed
 * @param init Init function (required)
 * @param update Update function (required)
 * @param cleanup Cleanup function, or NULL if none needed
 */
void NGStateRegister(u8 id, const char *name, NGStateInitFn init, NGStateUpdateFn update,
                     NGStateCleanupFn cleanup);
/** @} */

/** @name Enumeration */
/** @{ */

/**
 * Display name of a registered state.
 * @param id State id
 * @return Name, or NULL if unregistered or registered without one
 */
const char *NGStateGetName(u8 id);

/**
 * Number of registered states, for walking the list.
 * @return Count
 */
u8 NGStateCount(void);

/**
 * Id of the n-th registered state, in registration order.
 * @param index 0 to NGStateCount()-1
 * @return State id, or NG_STATE_ID_NONE if out of range
 */
u8 NGStateIdAt(u8 index);
/** @} */

/** @name Lifecycle */
/** @{ */

/**
 * Enter the given state as the current one. Call once at startup after
 * all states have been registered.
 * @param id State id to start
 */
void NGStateStart(u8 id);

/**
 * Advance the current state by one frame.
 * If the state's update function requests a switch, the current state is
 * cleaned up, the state arena (ng_arena_state) is reset, and the new
 * state is initialized - all before returning.
 */
void NGStateUpdate(void);

/**
 * Get the id of the currently running state.
 * @return Current state id, or NG_STATE_ID_NONE if NGStateStart() hasn't
 *         been called yet
 */
u8 NGStateGetCurrent(void);
/** @} */

/** @} */ /* end of state group */

#endif /* NG_STATE_H */
