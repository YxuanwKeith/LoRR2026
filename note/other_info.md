Problem Overview

r13
A team of robots works together in a simplified grid environment. Their job is to run infinite errands, which they accomplish by visiting different locations on the grid. Sometimes errands must be completed in a specific order. A sequence of such errands is called a task. The objective is to complete as many tasks as possible, as quickly as possible, until time runs out.

These types of problems are the core challenge in many real-world applications, including warehouse logistics, multi-robot manufacturing, multi-robot computer games and many more besides. The image below shows an example from a warehouse domain, which we call fulfilment.


image


On this page you can find further descriptions of the problem setting, the robot model, and the centralised controller that is responsible for plan validation and execution. More granular technical details are available in our competition Start-Kit.

Tasks, Errands and Assignments

r6
Before looking at how robots move, it is important to understand what they are trying to achieve.

An errand is a request for a specific robot to visit a particular target location on the grid. An errand is completed when the assigned robot arrives at the target location.

A task is a request for a specific robot to complete an ordered sequence of errands.

A task is open if one or more errands in the sequence have been completed.
A task is closed and no longer assigned when all the errands in the sequence have been completed.
Each robot has at most one open task.
Tasks can be assigned to any robot.

Once open, a task cannot be re-assigned.
When a task is completed, more tasks are revealed.
The objective is to complete as many tasks as possible over a given simulation period. Effective task assignment and path planning are crucial for achieving strong performance.

Illustrative Example:

There is a team of three robots, coloured blue, yellow, and green. The cells coloured pink represent potential errands: locations that might need to be visited by assigned robots.
description
Each robot is assigned a task (an ordered sequence of errands). In the image, the blue robot is assigned a task consisting of five errands, while the yellow and green robots are each assigned a task consisting of three errands. The arrows indicate the planned paths, which robots must follow to complete their errands.
description
The yellow robot has just completed the last errand in its assigned sequence. This task is now closed and the yellow robot becomes unassigned. The blue robot has one errand remaining. This task is considered open. The green robot is still trying to complete its first assigned errand. This task is neither open nor closed and could be re-assigned.
description
A new task is revealed and assigned to the yellow robot. This task contains four errands. The robots need to finish as many tasks as possible before a maximum time is reached. The total number of remaining (un-revealed) tasks is infinite.
description

The Dual-Timescale Architecture

To accomplish these tasks, the central controller operates on two different time scales. This is a key feature of the competition and a major departure from classic MAPF:

Planning updates (slower): The controller periodically communicates with the planner and task scheduler to receive updated assignments and high-level routes.
Execution ticks (faster): The controller advances the world every fine-grained time tick. It consults an executor to safely realize (or temporarily pause) the planned motions, while also injecting real-world delays.
image

Robots, Actions, and Execution Commands

r14
The environment is a grid map comprised of traversable and non-traversable cells (obstacles). It is deterministic, fully observable, and known ahead of time. Each robot occupies a single grid cell and has a designated orientation.

A Crucial Difference from Classic MAPF: In classic MAPF, robots move from one cell to another in a single, discrete time step. In this competition, to simulate real-world logistics, movement is continuous and can be interrupted. We handle this by splitting decision-making into two levels: high-level Actions (the planner's intended path) and low-level Commands (the executor's tick-by-tick physical movement).

Time is divided into fine, unit-sized time ticks.

1. The Planner's View: Actions

The path planner decides the overall route by generating a sequence of intended actions for each robot. This sequence is called a plan.

Unlike classic MAPF, actions here take time to complete. Each action has a minimum duration of d time ticks. The available actions are:

FW: Move Forward into an adjacent grid cell (d >= 10)
CR: Rotate 90 degrees clockwise (d >= 10)
CCR: Rotate 90 degrees counter-clockwise (d >= 10)
W: Wait at the current location (d >= 1)
Forward	Rotate
image	image
2. The Executor's Reality: Commands and Ticks

Because the simulator tracks real-time execution, robots do not instantly teleport to the next cell. Instead, they move together in parallel, progressing through their planned actions tick-by-tick.

To track the execution progress of an action, each robot maintains two integers:

Counter: The number of successful execution steps (ticks) the robot has completed for the current action.
Max Counter (d): The total number of steps required to complete the action and progress to the next grid location or orientation.
When a robot is ready to move, its planned actions are placed into a queue (they are staged). To actually physically execute the staged action, the executor issues the robot a command at every single time tick:

GO: Allow the robot to progress the current action by incrementing the counter by one.
STOP: Pause the robot; progress does not increase the counter.
On each tick, if the robot receives a GO command, its counter increments. When the counter reaches the Max Counter, the action is complete, the counter resets, and the robot snaps to the center of its new grid cell. If the counter is anywhere between 0 and the Max Counter, the robot is considered mid-action ("on the edge" or "in rotation").

Commitment and Pausing: Once a robot begins an action, it is committed to finishing it; it cannot start a different action halfway through. However, if the executor issues a STOP command (due to delays or to prevent a collision), the robot safely pauses its execution mid-action until it receives a GO command again.

action with tick
Each action has a duration of `d` “ticks”, which is the max counter. We measure the progress of the robot through the action by counting the number of elapsed ticks.
Execution Dynamics: Motions, Collisions, and Delays

To support realistic execution, the simulator maintains a continuous “real position” for robots during action execution. The executor enforces safety at every tick, adjusting the GO and STOP commands based on geometric realities and random delays.

Motions and Collisions

During a Forward action, the robot moves gradually from the centre of its current cell toward the centre of the next cell over time ticks. Intuitively, if the max counter is d, then at each tick, it advances about 1/d of a cell. During Rotate actions, the robot does not translate (it stays within its current cell while turning). When an action completes, the robot snaps back to a discrete state at a cell center (and the location/orientation is updated accordingly).

This competition does not use discrete MAPF “vertex collisions” or “edge collisions”. Instead, collisions are defined using geometric overlap:

Each robot is wrapped by a square safety bubble. The size of this square is the same as the robot's size and is a competition parameter. Note that the safety bubble does not rotate with the robot's orientation. The square safety bubble prevents execution deadlocks, compared with the round safety bubble.
Obstacles are treated as solid unit squares.
A collision occurs if any of the following happens:

Two robots’ safety squares overlap.
A robot’s safety square overlaps an obstacle square.
The executor enforces safety at every tick. If executing a robot’s next action step would cause a collision (with another robot, an obstacle, or the boundary), the executor may override that robot’s progress for that tick and make it wait (STOP) instead.

image

Delays

This competition models execution uncertainty. Robots may experience execution delays, which means the robot temporarily cannot make progress, even if it has a valid planned action. Delays do not change the planned action itself; they simply force a pause.

At any given tick, each robot has a probability of p to experience a delay event. When a delay event happens to a robot, that robot is forced to STOP (Wait) at its current location for a number of clock ticks, pausing its action progress. Note that when a robot is already in a delay, it will not trigger a new delay event.

Central Controller Components

The central controller is responsible for the correct operation of robots in the environment. It relies on three main user-implemented components to manage the dual-timescale architecture.

1. Task Scheduling

The role of the task scheduler is to specify a valid next task (or no task) for each robot at each planning update. An assignment is valid if every assigned task is a revealed task which has not been previously opened or closed by another robot.

When the controller calls the planning entry, the task scheduler is called first to determine the schedule, given:

The robot predicted states (starting state for this planning episode).
New tasks and new free robots (robots that finished their previously assigned tasks) from the last planning episode.
Below is an example of task assignments. The top row represents the task pool. The second row shows the robots. The left side illustrates an invalid assignment due to the following issues:

Task T5 is assigned to both robots A3 and A5, but each task can only be assigned to one robot.
Task T2, which is already assigned to another robot but not yet finished, is incorrectly assigned to robot A2.
Task T6 is assigned to a robot, which is invalid because tasks that are unrevealed, already completed, or nonexistent cannot be assigned.
description
2. Path Planner

To determine the intended actions for each robot, the controller relies on the path planner. The controller contacts the planner periodically (every multiple execution ticks), with a fixed time budget per call.

The planner returns a plan for each robot: a short-horizon sequence of grid-level actions (FW, CR, CCR, W). The planner does not output micro-actions or fractional movements. A “Forward” action is the planner’s high-level intent to move to the next cell; the executor handles the multi-tick realisation internally.

3. Executor

To safely apply a plan in the physical simulation under uncertainty, the controller relies on an executor that is called every execution tick.

A. Receive and Process a Plan from Planner When a new plan is received from the planner, the executor decides how to update the staged action queue for each robot. This processing can decide to adopt partial plans (cut from the middle) or replace old staged actions entirely, but it should not modify the actions themselves (e.g., it cannot delete, insert, or shuffle the orders of actions, except for wait actions).

After processing the new plan, the executor provides a “predicted state” for each robot. This represents an estimate of the robot's state at the end of the current plan execution, which gives the starting states for the planner to use next. It is only an estimate because actual execution may differ due to delays and collision avoidance.

B. Progress the Staged Action Queue (GO / STOP) At every tick, the execution policy decides what actually happens given:

The currently staged plan.
The current robot states (including mid-action states).
Delay situations from the last tick step (including if robots are in delay, and the time range it may delay for). For example, at tick=0, the robots don’t know whether they will have a delay; this delay information will be revealed at tick=1.
It outputs a GO or STOP command for each robot. If the executor issues GO, the robot increments the counter on its currently staged action. Once the counter reaches the maximum and the action is completed, it is removed from the top of the queue.

Timeout Control and Fallbacks

The central controller monitors elapsed wall-clock time. Time continues to pass while the scheduler, planner, and executor are running computations. After a predetermined simulation horizon, the controller stops and the problem instance is finished.

Controller's perspective on timeouts: Because the controller simulates in real-time, it will continue to advance the simulation ticks even when a component times out. robots may incur forced waits or continue executing outdated queues depending on the situation:

What happens if the task scheduler is late? If it does not complete in time, the existing assignment from the previous scheduling update is kept as the current assignment.
What happens if the task schedule is invalid? If the proposed assignment is invalid, the controller will set the invalid task schedule to -1. That means those robots with invalid schedules will have no task schedule, and the valid schedules for other robots will be kept.
What happens if the planner is late? The controller continues executing by asking the executor for the next moves based on the currently staged actions. If no actions are left in a robot's queue, that robot will wait in place.
What happens if the executor is late? If it is late processing a new plan, it is treated like a planner timeout (robots continue on existing queues). If it is late deciding the tick-by-tick GO/STOP commands, the controller will issue wait (STOP) commands for all robots until the executor returns.
What happens if a plan has collisions? The controller translates executor instructions into movements and checks for overlaps. We do not distinguish between bad plans and execution-level overlaps caused by delays. If any collision is detected, the colliding robots will safely stop and stay at their current locations. This process propagates until no robots are in collision at the current time tick.
Please refer to our competition Start-Kit for more details about interactions between the scheduler, planner, and executor, and detailed behaviour of the default implementations.