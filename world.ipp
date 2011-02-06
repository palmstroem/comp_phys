#include "interaction.hpp"

namespace trivial
{

    namespace
    {
        template <typename Vector, typename Rng>
        Vector generate_random_vector(Rng& generator)
        {
            auto index = 
                std::uniform_int<unsigned>()(generator, Vector::dimension * 2);

            return (index % 2 == 1 ? 1 : -1) * 
                    get_unit_vector<Vector>(index / 2);
        }

        template <typename T>
        void remove_element (T& vec, std::size_t index)
        {
            if (index < vec.size() - 1)
            {
                vec[index] = vec.back();
            }
            vec.pop_back();
        }
    }

    template <class P, class B, class Rng>
    void world<P, B, Rng>::step()
    {
        bath_.step(particles_, clusters_);

        // TODO: Probability matrix for the particle!

        // Order matters here!
        for (unsigned i = 0; i < particles_.size(); ++i)
        {
            std::vector<std::size_t> particles_to_join;
            std::vector<std::size_t> clusters_to_join;

            // Collect particles to join with the i-th one
            for (unsigned j = 0; j < particles_.size(); ++j)
            {
                if (i == j)
                    continue;

                auto result = interact(particles_[i], particles_[j]);
                if (result == interaction::MERGE)
                {
                    particles_to_join.push_back(j);
                }
            }

            // Collect clusters to join with
            for (unsigned k = 0; k < clusters_.size(); ++k)
            {
                auto result = interact(particles_[i], clusters_[k]);
                if (result == interaction::MERGE)
                {
                    clusters_to_join.push_back(k);
                }
            }
            
            // If we don't merge the particle with anything we move it
            if (clusters_to_join.empty() && particles_to_join.empty())
            {
                particles_[i].move(generate_random_vector<vector_type>(gen_));
                continue;
            }

            cluster_type* cl;

            if (clusters_to_join.empty())
            {
                clusters_.push_back(cluster_type());
                cl = &clusters_.back();
            }
            else
                cl = &(clusters_[clusters_to_join[0]]);

            cl->add_particle(particles_[i]);
            remove_element(particles_, i);

            // Reverse iteration because otherwise we would move
            // clusters that still have to be merged
            BOOST_REVERSE_FOREACH( std::size_t index, clusters_to_join )
            {
                if (cl == &clusters_[index])
                    continue;
                cl->merge(clusters_[index]);
                remove_element(clusters_, index);
            }

            BOOST_REVERSE_FOREACH( std::size_t index, particles_to_join )
            {
                // If we are at the former back-element we see that it has
                // moved to particles_[i]
                std::size_t to_remove = index;
                if (index == particles_.size())
                    to_remove = i;

                cl->add_particle(particles_[to_remove]);
                remove_element(particles_, to_remove);
            }
        }

        for (unsigned i = 0; i < clusters_.size(); ++i)
        {
            std::vector<std::size_t> clusters_to_join;

            for (unsigned j = 0; j < clusters_.size(); ++j)
            {
                // This is intended! We split clusters in bath_.step
                if (i == j) continue;
                auto result = interact(clusters_[i], clusters_[j]);
                if (result == interaction::MERGE)
                {
                    print("MERGE");
                    clusters_to_join.push_back(j);
                }
            }                    

            BOOST_REVERSE_FOREACH( std::size_t index, clusters_to_join )
            {
                clusters_[i].merge(clusters_[index]);
                clusters_[index] = clusters_.back();
                clusters_.pop_back();
            }

            clusters_[i].move(generate_random_vector<vector_type>(gen_));
        }
    }
}