#ifndef TRIVIAL_STATISTICS_VISITOR_HPP
#define TRIVIAL_STATISTICS_VISITOR_HPP

#include <iostream>

#include "visitor.hpp"
#include "vector.hpp"
#include "print.hpp"

namespace trivial
{
    namespace stat
    {
        template <class World>
        struct particles
        {
            static void write_header(std::ostream& out)
            { out << "particles"; }

            static void write_value (std::ostream& out,
                                     const typename World::cluster_type& c)
            { out << c.get_size(); }
        };

        template <class World>
        struct coord_number
        {
            static void write_header (std::ostream& out)
            { out << "avg. coord. no."; }

            static void write_value(std::ostream& out,
                                    const typename World::cluster_type& c)
            {
                float coord = 0;

                for (unsigned n = 0; n < c.get_size(); ++n)
                    for (unsigned m = 0; m < 2 * World::dimension; ++m)
                        if (c.has_particle_at (c.abs_position(c.get_particles()[n])
                            + (m % 2 ? 1 : -1)
                            * get_unit_vector<typename World::position_type>(m / 2)))
                            ++coord;

                out << coord / c.get_size();
            }
        };

        template<class World, unsigned Percent>
        struct radius_of_gyration
        {
            typedef typename World::position_type::float_vector_type flt_vec_t;

            static void write_header(std::ostream & out)
            { out << "radius of gyration (" << Percent << "%)"; }

            static void write_value(std::ostream& out,
                                    const typename World::cluster_type& c)
            {
                flt_vec_t center;

                // this relies on the particles being sorted by age ascending
                // in cluster!
                for(unsigned m = c.get_size() * (100 - Percent) / 100;
                    m < c.get_size();
                    ++m)
                {
                    center += flt_vec_t(c.get_particles()[m].position);
                }

                center /= c.get_size();

                float rog = 0;
                for(unsigned m = 0; m < c.get_size(); ++m)
                    rog += abs2(flt_vec_t(c.get_particles()[m].position) - center);

                out << std::sqrt(rog / c.get_size());
            }
        };

        template <class World, unsigned Bins>
        struct density
        {
            typedef typename World::position_type::float_vector_type flt_vec_t;

            static void write_header(std::ostream & out)
            { out << "density"; }

            static void write_value(std::ostream & out,
                                    const typename World::cluster_type & c)
            {
                const float step = c.get_radius() / Bins;
                out << "{ ";
                for(unsigned d = 0; d < Bins - 1; ++d)
                    out << get_dens(c, step * d, step * (d + 1)) << ", ";
                out << get_dens(c, step * (Bins - 1), step * Bins) << " }";
            }

            private:
                static float get_dens(const typename World::cluster_type & c,
                                     float dmin, float dmax)
                {
                    float d = 0;
                    for (unsigned n = 0; n < c.get_size(); ++n)
                    {
                        const float dist = abs(c.get_particles()[n].position - c.get_center());
                        if(dist >= dmin && dist < dmax)
                            ++d;
                    }
                    return d / (M_PI * (dmax * dmax - dmin * dmin));
                }
        };

        template <class World>
        struct dens_dens_correlation
        {
            typedef typename World::position_type::float_vector_type flt_vec_t;

            static void write_header(std::ostream & out)
            { out << "density density correlation"; }

            static void write_value(std::ostream & out,
                                    const typename World::cluster_type & c)
            {
                out << "{ ";
                for(unsigned d = 0; d < 2 * c.get_radius() - 1; ++d)
                    out << get_ddc(c, d) << ", ";
                out << get_ddc(c, 2 * c.get_radius() - 1) << " }";
            }

            private:
                static float get_ddc(const typename World::cluster_type & c,
                                     unsigned dist)
                {
                    float d = 0;
                    // only sum over occupied sites (other contribute 0)
                    for (unsigned n = 0; n < c.get_size(); ++n)
                        // average over all directions
                        for (unsigned m = 0; m < 2 * World::dimension; ++m)
                            if (c.has_particle_at(
                                    c.abs_position(c.get_particles()[n])
                                   + dist * (m % 2 ? 1 : -1)
                                   * get_unit_vector<typename World::position_type>(m / 2))
                               )
                                ++d;
                    return d / (2 * World::dimension * c.get_size());
                }
        };

        template<class World, unsigned Bins>
        struct score_dist
        {
            typedef typename World::position_type::float_vector_type flt_vec_t;

            static void write_header(std::ostream & out)
            { out << "max. score\tavg. score\tscore distribution"; }

            static void write_value(std::ostream & out, const typename World::cluster_type & c)
            {
                float score_max = 0;
                float score_avg = 0;
                for(unsigned n = 0; n < c.get_size(); ++n)
                {
                    score_max = std::max(score_max, c.get_particles()[n].score);
                    score_avg += c.get_particles()[n].score;
                }
                score_avg /= c.get_size();

                std::vector<unsigned> bins(Bins);
                for(unsigned n = 0; n < c.get_size(); ++n)
                    ++bins[c.get_particles()[n].score / score_max * (Bins - 1)];
    
                out << score_max << "\t" << score_avg << "\t{ ";
                for(unsigned n = 0; n < bins.size() - 1; ++n)
                    out << bins[n] << ", ";
                out << bins.back() << " }";
            }
        };
    }

    template <typename... Any> class statistics_visitor;

    template <typename World, typename Head, typename... Tail>
    class statistics_visitor<World, Head, Tail...> 
        : public statistics_visitor<World, Tail...>
    {
        typedef statistics_visitor<World, Tail...> base;

    public:
        statistics_visitor(std::ostream & out) : base(out) {}

        void visit(const World & world)
        {
            if(!base::header_)
            {
                base::out_ << std::endl << std::endl << "# Step\tCluster\t";
                write_header();
                base::header_ = true;
            }

            for(unsigned n = 0; n < world.get_clusters().size(); ++n)
            {
                base::out_ << base::step_ << "\t" << n << "\t";
                write_values(world.get_clusters()[n]);
            }

            ++base::step_;
        }

    protected:
        void write_header()
        {
            Head::write_header(base::out_);
            base::out_ << "\t";
            base::write_header();
        }

        void write_values(const typename World::cluster_type & c)
        {
            Head::write_value(base::out_, c);
            base::out_ << "\t";
            base::write_values(c);
        }
    };

    template <class World>
    class statistics_visitor<World> : public const_visitor<World>
    {
    public:
        statistics_visitor(std::ostream & out) : out_(out),
                                                 header_(false),
                                                 step_(1)
        {}

    protected:
        void write_header()
        { out_ << std::endl; }

        void write_values(const typename World::cluster_type & c)
        { out_ << std::endl; }

        std::ostream & out_;
        bool header_;
        unsigned step_;
    };
}

#endif
