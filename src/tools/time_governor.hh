/*!
 *
﻿ * Copyright (C) 2015 Technical University of Liberec.  All rights reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 3 as published by the
 * Free Software Foundation. (http://www.gnu.org/licenses/gpl-3.0.en.html)
 * 
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * 
 * @file    time_governor.hh
 * @brief   Basic time management class.
 * @author  Jan Brezina
 */

#ifndef TIME_HH_
#define TIME_HH_

#include <limits>
#include <cmath>
#include <algorithm>
#include <fstream>
#include <boost/circular_buffer.hpp>

#include <iosfwd>
#include <string>

#include "system/global_defs.h"
#include "system/system.hh"
#include "system/file_path.hh"
#include "input/accessors_forward.hh"
#include "input/input_exception.hh"
#include "system/exc_common.hh"
#include "system/exceptions.hh"
#include "tools/time_marks.hh"

namespace Input {
    class Record;
    class Tuple;
    template<class T> class Iterator;
    namespace Type {
        class Record;
        class Tuple;
    }
}



/**
 * @brief Helper class storing unit conversion coefficient and functionality for conversion of units.
 *
 * This class has created one instance for each TimeGovernor object. This object is shared with all
 * TimeSteps.
 */
class TimeUnitConversion {
public:
	// Constructor set coef_ from user defined unit
	TimeUnitConversion(std::string user_defined_unit);

	// Default constructor
	TimeUnitConversion();

    /**
     * Read and return time value multiplied by coefficient of given unit or global coefficient of equation
     * stored in coeff_. If time Tuple is not defined (e. g. Tuple is optional key) return default_time value.
     */
	double read_time(Input::Iterator<Input::Tuple> time_it, double default_time) const;

    /**
     * Read and return time unit coefficient given in unit_it or global coefficient of equation stored
     * in coeff_, if iterator is not defined.
     */
	double read_coef(Input::Iterator<std::string> unit_it) const;

    /**
     * Return global time unit coefficient of equation stored in coeff_.
     */
	inline double get_coef() const {
		return coef_;
	}

    /**
     * Return string representation of global time unit.
     */
	inline std::string get_unit_string() const {
		return unit_string_;
	}

protected:
    /// Conversion coefficient of all time values within the equation.
	double coef_;

	/// String representation of global unit of all time values within the equation.
	std::string unit_string_;

};



/**
 * @brief Representation of one time step.\
 *
 * Time step consists of the time step @p length() and from time step @end() time.
 * More over we store the index of the time step within it time governor.
 *
 * The reason to store both the end time and the length of the time step is to allow
 * safe comparisons of the time with safety margin small relative to the
 * time step length.
 */
class TimeStep {
public:
    /**
     * Constructor of the zero time step.
     */
    TimeStep(double init_time, std::shared_ptr<TimeUnitConversion> time_unit_conversion = std::make_shared<TimeUnitConversion>());

    /**
     * Default constructor.
     * Creates undefined time step.
     */
    TimeStep();

    /**
     * Copy constructor.
     */
    TimeStep(const TimeStep &other);

    /**
     * Create subsequent time step.
     */
    TimeStep make_next(double new_length) const;

    /**
     * Create subsequent time step, with the @end_time
     * explicitly specified. This allow slight discrepancy to
     * overcome rounding errors in the case of fixed time step.
     * Otherwise using small fixed time step, we may miss long term fixed
     * goal time.
     *
     */
    TimeStep make_next(double new_lenght, double end_time) const;

    /**
     * Getters.
     */
    unsigned int index() const {return index_;}
    double length() const { return length_;}
    double end() const { return end_;}
    /**
     * Performs rounding safe comparison time > other_time, i.e. time is strictly greater than given parameter
     * other_time with precision relative to the magnitude of the numbers time step.
     * TODO: introduce type TimeDouble with overloaded comparison operators, use it consistently in TimeMarks.
     */
    inline bool gt(double other_time) const
        { return ! safe_compare(other_time, end());}

    /**
     * Performs rounding safe comparison time >= other_time See @fn gt
     */
    inline bool ge(double other_time) const
        { return safe_compare(end(), other_time); }

    /**
     * Performs rounding safe comparison time < other_time. See @fn gt
     */
    inline bool lt(double other_time) const
        { return ! safe_compare(end(), other_time); }

    /**
     * Performs rounding safe comparison time <= other_time. See @fn gt
     */
    inline bool le(double other_time) const
        { return safe_compare(other_time, end()); }

    /**
      * Performs rounding safe comparison time (step) == other_time. See @fn gt
      */
    inline bool eq(double other_time) const
        { return this->le(other_time) && this->ge(other_time); }

    inline bool contains(double other_time) const
        { return this->ge(other_time) && this->lt(other_time + length_); }

    /**
     * Read and return time value multiplied by coefficient of given unit or global coefficient of equation
     * stored in time_unit_conversion_. If time Tuple is not defined (e. g. Tuple is optional key) return
     * default_time value.
     */
    double read_time(Input::Iterator<Input::Tuple> time_it, double default_time=std::numeric_limits<double>::quiet_NaN()) const;

    /**
     * Read and return time unit coefficient given in unit_it or global coefficient of equation stored
     * in time_unit_conversion_, if iterator is not defined.
     */
	double read_coef(Input::Iterator<std::string> unit_it) const;

    /**
     * Return global time unit coefficient of equation stored in coeff_.
     */
	double get_coef() const;

    /**
     * Returns true if two time steps are exactly the same.
     */
    bool operator==(const TimeStep & other)
        { return (index_ == other.index_)
                && (length_ == other.length_)
                && (end_ == other.end_);
        }
private:

    /* Returns true if t1-t0 > delta. Where delta is choosen
     * related to the current time step and magnitude of t1, t0.
     */
    bool safe_compare(double t1, double t0) const;

    /// Index of the step is index if the end time. Zero time step is artificial.
    unsigned int index_;
    /// Length of the time step. Theoretically @p end minus end of the previous time step.
    /// However may be slightly different due to rounding errors.
    double length_;
    /// End time point of the time step.
    double end_;
    /// Conversion unit of all time values within the equation.
    std::shared_ptr<TimeUnitConversion> time_unit_conversion_;
};

std::ostream& operator<<(std::ostream& out, const TimeStep& t_step);



/**
 * @brief
 * Basic time management functionality for unsteady (and steady) solvers (class Equation).
 *
 * <h2> Common features and unsteady time governor (TG) </h2>
 * 
 * This class provides algorithm for selecting next time step, and information about current time step frame.
 * Step estimating is constrained by several bounds (permanent maximal and minimal time step, upper 
 * and lower constraint of time step). The permanent constraints are set in the constructor from the input 
 * record so that user can set the time step constraints for the whole simulation.
 * Function set_dt_limits() should be used only in very specific cases and possibly right after
 * the constructor before using other functions of TG.
 * 
 * Choice of the very next time step can be constrained using functions set_upper_constraint()
 * and set_lower_constraint().
 * Lower and upper constraints are set equal to permanent ones in the constructor and can only 
 * become stricter. If one tries to set these constraints outside the interval of the previous constraints,
 * nothing is changed and a specified value is returned. Upper and lower constraints are reset in function
 * next_time() to the permanent constraints.
 * 
 * The later one can be called multiple times with various constraint values and we use the minimum of them.
 * Function next_time() choose the next time step in such a way that it meets actual constraints and
 * a uniform discrete time grid with this step hits the nearest fixed time in lowest possible number of steps.
 *
 * The fixed times are time marks of TimeMarks object passed at construction time with particular mask.
 *
 * There is just one set of time marks for the whole problem. Therefore TimeMarks object is static and is shared umong
 * all the equations and time governors. Each equation creates its own specific time mark type.
 *
 * Information provided by TG includes:
 * - actual time, last time, end time
 * - actual time step
 * - number of the time level
 * - end of interval with fixed time step
 * - time comparison
 * - static pointer to time marks
 * 
 * <h2> Steady time governor</h2>
 * 
 * Steady TG can be constructed by default constructor (initial time is zero) or by 
 * constructor with initial time as parameter. End time and time step are set to infinity. 
 * One can check if the time governor is steady by calling is_steady(). 
 * Calling estimate_dt() will return infinity.
 * 
 * Setting constraints have no consequences. Calling fix_dt_until_mark() will only return zero 
 * and will not do anything.
 * 
 * The steady TG works in two states. At first the time is set to initial and time level 
 * is equal zero. To use steady TG properly one should call next_time() after the computation 
 * of steady problem is done. Current time is then set to infinity, time level is set to 1 and 
 * calling estimate_dt() will return zero.
 * 
 * Note: For example class TransportNothing (which computes really nothing) uses also steady TG but
 * it calls next_time() immediately after TG's construction. This means that the 'computation'of transport 
 * is done.
 * 
 *
 */

class TimeGovernor
{
public:

	DECLARE_INPUT_EXCEPTION(ExcTimeGovernorMessage, << EI_Message::val);
	TYPEDEF_ERR_INFO( EI_Index, int);
	TYPEDEF_ERR_INFO( EI_BackIndex, unsigned int);
	TYPEDEF_ERR_INFO( EI_HistorySize, unsigned int);
	DECLARE_EXCEPTION(ExcMissingTimeStep,
	        << "Time step index: " << EI_Index::val
	        << ", history index: " << EI_BackIndex::val
	        << " out of history of size: " << EI_HistorySize::val);

    static const Input::Type::Record & get_input_type();

    static const Input::Type::Tuple & get_input_time_type(double lower_bound= -std::numeric_limits<double>::max(),
                                                          double upper_bound=std::numeric_limits<double>::max());

    /**
     * Getter for time marks.
     */
    static inline TimeMarks &marks()
            {return time_marks_;}
    
    /**
     * @brief Constructor for unsteady solvers.
     *
     * @param input accessor to input data
     * @param fixed_time_mask TimeMark mask used to select fixed time marks from all the time marks. 
     * @param timestep_output enable/forbid output of time steps to YAML file
     * This value is bitwise added to the default one defined in TimeMarks::type_fixed_time().
     *
     */
   TimeGovernor(const Input::Record &input,
                TimeMark::Type fixed_time_mask = TimeMark::none_type,
				bool timestep_output = true);

   /**
    * @brief Default constructor - steady time governor.
    * 
    * OBSOLETE.
    *
    * We can have "zero step" steady problem (no computation, e.g. EquationNothing) and one step steady problem
    * (e.g. steady water flow).
    * 
    * Time is set to zero, time step and end time to infinity.
    * 
    * First call of next_time() pushes the actual time to infinity.
    * 
    * However, you have to use full constructor for the "steady problem" that has time-variable input data.
    * 
    * Has a private pointer to static TimeMarks and can access them by marks().
    */
    explicit TimeGovernor(double init_time=0.0,
		   	    TimeMark::Type fixed_time_mask = TimeMark::none_type);

   /**
    * The aim of this constuctor is simple way to make a time governor without Input interface.
    *
    * TODO: Partially tested as part of field test. Needs its own unit test.
    */
   TimeGovernor(double init_time, double dt);

	/**
	 * Destructor.
	 */
	~TimeGovernor();

   /**
    * Returns true if the time governor was set from default values
    */
   bool is_default() {
       return (end_time_ == max_end_time)
               && (max_time_step_ == end_time_ - init_time_);
   }

   /**
    * @brief Sets dt limits for time dependent DT limits in simulation.
    *
    * This function should not be normally used. These values are to be set in constructor
    * from the input record or by default.
    * @param min_dt is the minimal value allowed for time step
    * @param max_dt is the maximal value allowed for time step
    * @param dt_limits_list list of time dependent values of minimal and maximal value allowed for time step
    */
   void set_dt_limits( double min_dt, double max_dt, Input::Array dt_limits_list);

    /**
     * @brief Sets upper constraint for the next time step estimating.
     * 
     * This function can only make the constraint stricter. Upper constraint is reset to @p max_dt after the next_time() call.
     * The return value mimics result of the comparison: current constraint  compared to  @param upper.
     * @param message describes the origin of the constraint
     * In particular the return values is:
     * - -1: Current upper constraint is less then the @param upper. No change happen.
     * -  0: Current constraint interval contains the @param upper. The upper constraint is set.
     * - +1: Current lower constraint is greater then the @param upper. No change happen.
     */
    int set_upper_constraint(double upper, std::string message);
    
    /**
     * @brief Sets lower constraint for the next time step estimating.
     *
     * This function can only make the constraint stricter. Lower constraint is reset to @p min_dt after the next_time() call.
     * The return value mimics result of the comparison: current constraint  compared to  @param upper.
     * In particular the return values is:
     * - -1: Current upper constraint is less then the @param lower. No change happen.
     * -  0: Current constraint interval contains the @param lower. The lower constraint is set.
     * - +1: Current upper constraint is greater then the @param lower. No change happen.
     */
    /**
     * @brief Sets lower constraint for the next time step estimating. 
     * @param lower is the lower constraint for time step
     * @param message describes the origin of the constraint
     * @return -1, 0 or 1 according to the success.
     * @see set_upper_constrain().
     */
    int set_lower_constraint(double lower, std::string message);
    
    /**
     * @brief Fixing time step until fixed time mark.
     * 
     * Fix time step until first fixed time mark. When called inside an already fixed interval, 
     * it overwrites previous setting. 
     * @return actual end of fixed time step.
     */
    double fix_dt_until_mark();

    /**
     * @brief Proceed to the next time according to current estimated time step.
     *
     * The timestep constraints are relaxed to the permanent constraints.
     */
    void next_time();

    /**
     * @brief Force timestep reduction in particular in the case of failure of the non-linear solver.
     *
     * Calling this method also force immediate end of the fixed timestep interval.
     * Returns true reduce factor used. It is larger then given factor if we hit the lower timestep constraint.
     *
     * TODO: How to keep constraints active for the last next_time call.
     */
    double reduce_timestep(double factor);

    /**
     *  Returns reference to required time step in the recent history.
     *  Without parameter the actual time step is returned.
     *  Use negative indices to get recent time steps: step(-1) the actual step, step(-2) the last one.
     *  Use positive index to get time step by its index: step(0) the first time step.
     *  However only limited number of last time steps is stored.
     *  If the time step is not accessible any more, we throw an exception ExcMissingTimeStep.
     */
    const TimeStep &step(int index=-1) const;

    /**
     *	Specific time mark of the equation owning the time governor.
     */
    inline TimeMark::Type equation_mark_type() const
    { return eq_mark_type_;}

    /**
     *	Specific time mark of the fixed times of the equation owning the time governor.
     */
    inline TimeMark::Type equation_fixed_mark_type() const
    { return eq_mark_type_ | marks().type_fixed_time(); }

    /**
     * Add sequence of time marks starting from the initial time up to the end time with given @p step.
     * Time marks type combines given mark_type (none by default) and native mark type of the time governor.
     */
    void add_time_marks_grid(double step, TimeMark::Type mark_type= TimeMark::none_type) const;

    /**
     * Simpler interface to TimeMarks::is_current().
     */
    bool is_current(const TimeMark::Type &mask) const;


    /**
     * Simpler interface to TimeMarks::next().
     */
    inline TimeMarks::iterator next(const TimeMark::Type &mask) const
        {return time_marks_.next(*this, mask);}

    /**
     * Simpler interface to TimeMarks::last().
     */
    inline TimeMarks::iterator last(const TimeMark::Type &mask) const
        {return time_marks_.last(*this, mask);}

    /**
     *  Getter for upper constrain.
     */
    inline double upper_constraint() const
        {return upper_constraint_;}
    
    /**
     *  Returns lower constraint.
     */
    inline double lower_constraint() const
        {return lower_constraint_;}
        
    /**
     * End of interval with currently fixed time step. Can be changed by next call of method fix_dt_until_mark.
     */
    inline double end_of_fixed_dt() const
        {return end_of_fixed_dt_interval_;}

    /**
     *  Getter for dt_changed. Returns whether the time step has been changed.
     */
    inline bool is_changed_dt() const
        {return time_step_changed_;}


    /**
     * Initial time getter.
     */
    inline double init_time() const
        {return this->init_time_;}

    /**
     * End of actual time interval; i.e. where the solution is computed.
     */
    inline double t() const
        {return step().end();}

    /**
     * Previous time step.
     */
    inline double last_dt() const
        {if (step().index() >0) return step(-2).length();
        else return inf_time;
        }

    /**
     * Previous time.
     */
    inline double last_t() const
        {if (step().index() >0) return step(-2).end();
        else return step().end() - step().length();
        }


    /**
     * Length of actual time interval; i.e. the actual time step.
     */
    inline double dt() const
        {return step().length();}

    /**
     * @brief Estimate choice of next time step according to actual setting of constraints.
     * 
     * Precedence of constraints:
     *
     *  -# meet next fixed time (always satisfied)
     *  -# permanent upper constraint (always satisfied)
     *  -# upper constraint (always satisfied)
     *  -# lower constraint (satisfied if not in conflict with 1.)
     *  -# permanent lower constraint (satisfied if 4.)
     *  -# else writes the difference between lower constraint and estimated time step
     *  -# If there are more then one step to the next fixed time, try to
     *     use the last time step if it is nearly the same.
     */
    double estimate_dt() const;

    /**
     * Estimate next time.
     */
    inline double estimate_time() const
        {return t()+estimate_dt();}

    /// End time.
    inline double end_time() const
    { return end_time_; }

    /// Returns true if the actual time is greater than or equal to the end time.
    inline bool is_end() const
        { return (this->step().ge(end_time_) || t() == inf_time); }
        
    /// Returns true if the time governor is used for steady problem.
    inline bool is_steady() const
    { return steady_; }



    /**
     * Returns the time level.
     */
    inline int tlevel() const
        {return step().index();}

    /**
     * Prints output of TimeGovernor.
     * @param name is the name of time governor that you want to show up in output (just for your convenience)
     *
     */
    void view(const char *name="") const;

    /**
     * Read and return time value multiplied by coefficient of given unit or global coefficient of equation
     * stored in time_unit_conversion_. If time Tuple is not defined (e. g. Tuple is optional key) return
     * default_time value.
     */
    double read_time(Input::Iterator<Input::Tuple> time_it, double default_time=std::numeric_limits<double>::quiet_NaN()) const;

    /**
     * Read and return time unit coefficient given in unit_it or global coefficient of equation stored
     * in time_unit_conversion_, if iterator is not defined.
     */
	double read_coef(Input::Iterator<std::string> unit_it) const;

    /**
     * Return global time unit coefficient of equation stored in coeff_.
     */
	double get_coef() const;

    /**
     * Return string representation of global time unit.
     */
	std::string get_unit_string() const;

	// Maximal tiem of simulation. More then age of the universe in seconds.
    static const double max_end_time;

    /// Infinity time used for steady case.
    static const double inf_time;

    /**
     *  Rounding precision for computing time_step.
     *  Used as technical lower bound for the time step.
     */
    static const double time_step_precision;

private:

    /// Structure that stores one record of DT limit.
    struct DtLimitRow {

    	DtLimitRow(double t, double min, double max)
        : time(t),
		  min_dt(min),
		  max_dt(max)
      {};

      double time;    ///< time of DT limits record
      double min_dt;  ///< min DT limit
      double max_dt;  ///< max DT limit
    };

    /**
     * \brief Common part of the constructors. Set most important parameters, check they are valid and set default values to other.
     *
     * Set main parameters to given values.
     * Check they are correct.
     * Set soft and permanent constrains to the same, the least restricting values.
     * Set time marks for the start time and end time (if finite).
     */
    void init_common(double init_time, double end_time, TimeMark::Type type);

    /**
     * \brief Sets permanent constraints for actual time step.
     */
    void set_permanent_constraint();

    /**
     *  Size of the time step buffer, i.e. recent_time_steps_.
     */
    static const unsigned int size_of_recent_steps_ = 3;

    /// Circular buffer of recent time steps. Implicit size is 3.
    boost::circular_buffer<TimeStep> recent_steps_;
    /// Initial time.
    double init_time_;
    /// End of interval if fixed time step.
    double end_of_fixed_dt_interval_;
    /// End time of the simulation.
    double end_time_;

    /// Next fixed time step.
    double fixed_time_step_;
    /// Flag that is set when the fixed step is set (lasts only one time step).
    bool is_time_step_fixed_;
    /// Flag is set if the time step has been changed (lasts only one time step).
    bool time_step_changed_;
    
    /// Description of the upper constraint.
    std::string upper_constraint_message_;
    /// Description of the upper constraint.
    std::string lower_constraint_message_;
    /// Upper constraint for the choice of the next time step.
    double upper_constraint_;
    /// Lower constraint for the choice of the next time step.
    double lower_constraint_;
    /// Permanent upper limit for the time step.
    double max_time_step_;
    /// Permanent lower limit for the time step.
    double min_time_step_;

    /// Upper constraint used for choice of current time.
    double last_upper_constraint_;
    /// Lower constraint used for choice of current time.
    double last_lower_constraint_;

    /**
     * When the next time is chosen we need only the lowest fix time. Therefore we use
     * minimum priority queue of doubles based on the vector container.
     * This is one global set of time marks for the whole problem and is shared among all equations.
     * Therefore this object is static constant pointer.
     */
    static TimeMarks time_marks_;
    
    /// TimeMark type of the equation.
    TimeMark::Type eq_mark_type_;
    
    /// True if the time governor is used for steady problem.
    bool steady_;

    /// Conversion unit of all time values within the equation.
    std::shared_ptr<TimeUnitConversion> time_unit_conversion_;

    /// Table of DT limits
    std::vector<DtLimitRow> dt_limits_table_;

    /// Index to actual position of DT limits
    unsigned int dt_limits_pos_;

    /// File path for timesteps_output_ stream.
    FilePath timesteps_output_file_;

    /// Handle for file for output time steps to YAML format.
    std::ofstream timesteps_output_;

    /// Store last printed time to YAML output, try multiplicity output of one time
    double last_printed_timestep_;

    /// Special flag allows forbid output time steps during multiple initialization of TimeGovernor
    bool timestep_output_;

    /// Allows add all times defined in dt_limits_table_ to list of TimeMarks
    bool limits_time_marks_;

    friend TimeMarks;
};

/**
 * \brief Stream output operator for TimeGovernor.
 *
 * Currently for debugging purposes.
 * In the future it should be customized for use in combination with
 * streams for various log targets.
 *
 */
std::ostream& operator<<(std::ostream& out, const TimeGovernor& tg);



#endif /* TIME_HH_ */
