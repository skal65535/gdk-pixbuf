/* Extensive GtkTreeModelFilter tests.
 * Copyright (C) 2009  Kristian Rietveld  <kris@gtk.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <gtk/gtk.h>


/*
 * Model creation
 */

#define LEVEL_LENGTH 5

static void
create_tree_store_set_values (GtkTreeStore *store,
                              GtkTreeIter  *iter,
                              gboolean      visible)
{
  GtkTreePath *path;

  path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), iter);
  gtk_tree_store_set (store, iter,
                      0, gtk_tree_path_to_string (path),
                      1, visible,
                      -1);
  gtk_tree_path_free (path);
}

static void
create_tree_store_recurse (int           depth,
                           GtkTreeStore *store,
                           GtkTreeIter  *parent,
                           gboolean      visible)
{
  int i;

  for (i = 0; i < LEVEL_LENGTH; i++)
    {
      GtkTreeIter iter;

      gtk_tree_store_insert (store, &iter, parent, i);
      create_tree_store_set_values (store, &iter, visible);

      if (depth > 0)
        create_tree_store_recurse (depth - 1, store, &iter, visible);
    }
}

static GtkTreeStore *
create_tree_store (int      depth,
                   gboolean visible)
{
  GtkTreeStore *store;

  store = gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_BOOLEAN);

  create_tree_store_recurse (depth, store, NULL, visible);

  return store;
}


/*
 * Fixture
 */

typedef struct
{
  GtkWidget *tree_view;

  GtkTreeStore *store;
  GtkTreeModelFilter *filter;
} FilterTest;

static void
filter_test_setup_generic (FilterTest    *fixture,
                           gconstpointer  test_data,
                           int            depth,
                           gboolean       empty,
                           gboolean       unfiltered)
{
  const GtkTreePath *vroot = test_data;
  GtkTreeModel *filter;

  fixture->store = create_tree_store (depth, !empty);

  /* Please forgive me for casting const away. */
  filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (fixture->store),
                                      (GtkTreePath *)vroot);
  fixture->filter = GTK_TREE_MODEL_FILTER (filter);

  if (!unfiltered)
    gtk_tree_model_filter_set_visible_column (fixture->filter, 1);

  /* We need a tree view that's listening to get ref counting from that
   * side.
   */
  fixture->tree_view = gtk_tree_view_new_with_model (filter);
}

static void
filter_test_setup (FilterTest    *fixture,
                   gconstpointer  test_data)
{
  filter_test_setup_generic (fixture, test_data, 3, FALSE, FALSE);
}

static void
filter_test_setup_empty (FilterTest    *fixture,
                         gconstpointer  test_data)
{
  filter_test_setup_generic (fixture, test_data, 3, TRUE, FALSE);
}

static void
filter_test_setup_unfiltered (FilterTest    *fixture,
                              gconstpointer  test_data)
{
  filter_test_setup_generic (fixture, test_data, 3, FALSE, TRUE);
}

static void
filter_test_setup_empty_unfiltered (FilterTest    *fixture,
                                    gconstpointer  test_data)
{
  filter_test_setup_generic (fixture, test_data, 3, TRUE, TRUE);
}

static void
filter_test_enable_filter (FilterTest *fixture)
{
  gtk_tree_model_filter_set_visible_column (fixture->filter, 1);
  gtk_tree_model_filter_refilter (fixture->filter);
}

static void
filter_test_teardown (FilterTest    *fixture,
                      gconstpointer  test_data)
{
  g_object_unref (fixture->filter);
  g_object_unref (fixture->store);
}

/*
 * Model structure validation
 */

static void
check_filter_model_recurse (FilterTest  *fixture,
                            GtkTreePath *store_parent_path,
                            GtkTreePath *filter_parent_path)
{
  int i;
  GtkTreeIter store_iter;
  GtkTreeIter filter_iter;
  gboolean store_has_next, filter_has_next;

  gtk_tree_path_down (store_parent_path);
  gtk_tree_path_down (filter_parent_path);

  store_has_next = gtk_tree_model_get_iter (GTK_TREE_MODEL (fixture->store),
                                            &store_iter, store_parent_path);
  filter_has_next = gtk_tree_model_get_iter (GTK_TREE_MODEL (fixture->filter),
                                             &filter_iter, filter_parent_path);

  for (i = 0; i < LEVEL_LENGTH; i++)
    {
      gboolean visible;

      g_return_if_fail (store_has_next == TRUE);

      gtk_tree_model_get (GTK_TREE_MODEL (fixture->store),
                          &store_iter,
                          1, &visible,
                          -1);

      if (visible)
        {
          GtkTreePath *tmp;
          gchar *filter_str, *store_str;

          g_return_if_fail (filter_has_next == TRUE);

          /* Verify path */
          tmp = gtk_tree_model_get_path (GTK_TREE_MODEL (fixture->filter),
                                         &filter_iter);
          g_return_if_fail (gtk_tree_path_compare (tmp, filter_parent_path) == 0);

          /* Verify model content */
          gtk_tree_model_get (GTK_TREE_MODEL (fixture->store),
                              &store_iter,
                              0, &store_str,
                              -1);
          gtk_tree_model_get (GTK_TREE_MODEL (fixture->filter),
                              &filter_iter,
                              0, &filter_str,
                              -1);

          g_return_if_fail (g_strcmp0 (store_str, filter_str) == 0);

          g_free (store_str);
          g_free (filter_str);

          if (gtk_tree_model_iter_has_child (GTK_TREE_MODEL (fixture->filter),
                                             &filter_iter))
            {
              g_return_if_fail (gtk_tree_model_iter_has_child (GTK_TREE_MODEL (fixture->store), &store_iter));

              check_filter_model_recurse (fixture,
                                          gtk_tree_path_copy (store_parent_path),
                                          tmp);
            }

          gtk_tree_path_next (filter_parent_path);
          filter_has_next = gtk_tree_model_iter_next (GTK_TREE_MODEL (fixture->filter), &filter_iter);
        }

      gtk_tree_path_next (store_parent_path);
      store_has_next = gtk_tree_model_iter_next (GTK_TREE_MODEL (fixture->store), &store_iter);
    }

  /* Both models should have no more content! */
  g_return_if_fail (store_has_next == FALSE);
  g_return_if_fail (filter_has_next == FALSE);

  gtk_tree_path_free (store_parent_path);
  gtk_tree_path_free (filter_parent_path);
}

static void
check_filter_model (FilterTest *fixture)
{
  GtkTreePath *path;

  path = gtk_tree_path_new ();

  check_filter_model_recurse (fixture, path, gtk_tree_path_copy (path));
}

static void
check_filter_model_with_root (FilterTest  *fixture,
                              GtkTreePath *path)
{
  check_filter_model_recurse (fixture,
                              gtk_tree_path_copy (path),
                              gtk_tree_path_new ());
}

/* Helpers */

static void
check_level_length (GtkTreeModelFilter *filter,
                    const gchar        *level,
                    const int           length)
{
  if (!level)
    {
      int l = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (filter), NULL);
      g_return_if_fail (l == length);
    }
  else
    {
      int l;
      gboolean retrieved_iter = FALSE;
      GtkTreeIter iter;

      retrieved_iter = gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (filter),
                                                            &iter, level);
      g_return_if_fail (retrieved_iter);
      l = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (filter), &iter);
      g_return_if_fail (l == length);
    }
}

static void
set_path_visibility (FilterTest  *fixture,
                     const gchar *path,
                     gboolean     visible)
{
  GtkTreeIter store_iter;

  gtk_tree_model_get_iter_from_string (GTK_TREE_MODEL (fixture->store),
                                       &store_iter, path);
  gtk_tree_store_set (fixture->store, &store_iter,
                      1, visible,
                      -1);
}

static void
insert_path_with_visibility (FilterTest  *fixture,
                             const gchar *path_string,
                             gboolean     visible)
{
  int position;
  GtkTreePath *path;
  GtkTreeIter parent, iter;

  path = gtk_tree_path_new_from_string (path_string);
  position = gtk_tree_path_get_indices (path)[gtk_tree_path_get_depth (path)];
  gtk_tree_path_up (path);

  if (gtk_tree_model_get_iter (GTK_TREE_MODEL (fixture->store), &parent, path))
    {
      gtk_tree_store_insert (fixture->store, &iter, &parent, position);
      create_tree_store_set_values (fixture->store, &iter, visible);
    }
  gtk_tree_path_free (path);
}

/*
 * The actual tests.
 */

static void
verify_test_suite (FilterTest    *fixture,
                   gconstpointer  user_data)
{
  check_filter_model (fixture);
}

static void
verify_test_suite_vroot (FilterTest    *fixture,
                         gconstpointer  user_data)
{
  check_filter_model_with_root (fixture, (GtkTreePath *)user_data);
}


static void
filled_hide_root_level (FilterTest    *fixture,
                        gconstpointer  user_data)
{
  set_path_visibility (fixture, "2", FALSE);
  check_filter_model (fixture);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH - 1);

  set_path_visibility (fixture, "0", FALSE);
  check_filter_model (fixture);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH - 2);

  set_path_visibility (fixture, "4", FALSE);
  check_filter_model (fixture);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH - 3);


  /* Hide remaining */
  set_path_visibility (fixture, "1", FALSE);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH - 4);

  set_path_visibility (fixture, "3", FALSE);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH - 5);

  check_filter_model (fixture);

  /* Show some */
  set_path_visibility (fixture, "1", TRUE);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH - 4);

  set_path_visibility (fixture, "3", TRUE);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH - 3);

  check_filter_model (fixture);
}

static void
filled_hide_child_levels (FilterTest    *fixture,
                          gconstpointer  user_data)
{
  set_path_visibility (fixture, "0:2", FALSE);
  check_filter_model (fixture);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH);
  check_level_length (fixture->filter, "0", LEVEL_LENGTH - 1);

  set_path_visibility (fixture, "0:4", FALSE);
  check_filter_model (fixture);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH);
  check_level_length (fixture->filter, "0", LEVEL_LENGTH - 2);

  set_path_visibility (fixture, "0:4:3", FALSE);
  check_filter_model (fixture);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH);
  check_level_length (fixture->filter, "0", LEVEL_LENGTH - 2);

  set_path_visibility (fixture, "0:4:0", FALSE);
  set_path_visibility (fixture, "0:4:1", FALSE);
  set_path_visibility (fixture, "0:4:2", FALSE);
  set_path_visibility (fixture, "0:4:4", FALSE);
  check_filter_model (fixture);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH);
  check_level_length (fixture->filter, "0", LEVEL_LENGTH - 2);

  set_path_visibility (fixture, "0:4", TRUE);
  /* Since "0:2" is hidden, "0:4" must be "0:3" in the filter model */
  check_filter_model (fixture);
  check_level_length (fixture->filter, "0:3", 0);

  set_path_visibility (fixture, "0:2", TRUE);
  check_filter_model (fixture);
  check_level_length (fixture->filter, "0:2", LEVEL_LENGTH);
  check_level_length (fixture->filter, "0:3", LEVEL_LENGTH);
  check_level_length (fixture->filter, "0:4", 0);

  set_path_visibility (fixture, "0:4:2", TRUE);
  set_path_visibility (fixture, "0:4:4", TRUE);
  check_level_length (fixture->filter, "0:4", 2);
}


static void
filled_vroot_hide_root_level (FilterTest    *fixture,
                              gconstpointer  user_data)
{
  GtkTreePath *path = (GtkTreePath *)user_data;

  /* These changes do not affect the filter's root level */
  set_path_visibility (fixture, "0", FALSE);
  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH);
  check_level_length (fixture->filter, "0", LEVEL_LENGTH);

  set_path_visibility (fixture, "4", FALSE);
  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH);
  check_level_length (fixture->filter, "0", LEVEL_LENGTH);

  /* Even though we set the virtual root parent node to FALSE,
   * the virtual root contents remain.
   */
  set_path_visibility (fixture, "2", FALSE);
  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH);
  check_level_length (fixture->filter, "0", LEVEL_LENGTH);

  /* No change */
  set_path_visibility (fixture, "1", FALSE);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH);
  check_level_length (fixture->filter, "0", LEVEL_LENGTH);

  set_path_visibility (fixture, "3", FALSE);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH);
  check_level_length (fixture->filter, "0", LEVEL_LENGTH);

  check_filter_model_with_root (fixture, path);

  /* Show some */
  set_path_visibility (fixture, "2", TRUE);
  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH);
  check_level_length (fixture->filter, "0", LEVEL_LENGTH);

  set_path_visibility (fixture, "1", TRUE);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH);
  check_level_length (fixture->filter, "0", LEVEL_LENGTH);

  set_path_visibility (fixture, "3", TRUE);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH);
  check_level_length (fixture->filter, "0", LEVEL_LENGTH);

  check_filter_model_with_root (fixture, path);

  /* Now test changes in the virtual root level */
  set_path_visibility (fixture, "2:2", FALSE);
  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH - 1);

  set_path_visibility (fixture, "2:4", FALSE);
  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH - 2);

  set_path_visibility (fixture, "1:4", FALSE);
  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH - 2);

  set_path_visibility (fixture, "2:4", TRUE);
  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH - 1);

  set_path_visibility (fixture, "2", FALSE);
  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH - 1);

  set_path_visibility (fixture, "2:0", FALSE);
  set_path_visibility (fixture, "2:1", FALSE);
  set_path_visibility (fixture, "2:2", FALSE);
  set_path_visibility (fixture, "2:3", FALSE);
  set_path_visibility (fixture, "2:4", FALSE);
  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, NULL, 0);

  set_path_visibility (fixture, "2", TRUE);
  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, NULL, 0);

  set_path_visibility (fixture, "1:4", FALSE);
  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, NULL, 0);

  set_path_visibility (fixture, "2:4", TRUE);
  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH - 4);

  set_path_visibility (fixture, "2:4", FALSE);
  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, NULL, 0);

  set_path_visibility (fixture, "2", FALSE);
  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, NULL, 0);

  set_path_visibility (fixture, "2:0", TRUE);
  set_path_visibility (fixture, "2:1", TRUE);
  set_path_visibility (fixture, "2:2", TRUE);
  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH - 2);

  set_path_visibility (fixture, "2", TRUE);
  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH - 2);
}

static void
filled_vroot_hide_child_levels (FilterTest    *fixture,
                                gconstpointer  user_data)
{
  GtkTreePath *path = (GtkTreePath *)user_data;

  set_path_visibility (fixture, "2:0:2", FALSE);
  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH);
  check_level_length (fixture->filter, "0", LEVEL_LENGTH - 1);

  set_path_visibility (fixture, "2:0:4", FALSE);
  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH);
  check_level_length (fixture->filter, "0", LEVEL_LENGTH - 2);

  set_path_visibility (fixture, "2:0:4:3", FALSE);
  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH);
  check_level_length (fixture->filter, "0", LEVEL_LENGTH - 2);

  set_path_visibility (fixture, "2:0:4:0", FALSE);
  set_path_visibility (fixture, "2:0:4:1", FALSE);
  set_path_visibility (fixture, "2:0:4:2", FALSE);
  set_path_visibility (fixture, "2:0:4:4", FALSE);
  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH);
  check_level_length (fixture->filter, "0", LEVEL_LENGTH - 2);

  set_path_visibility (fixture, "2:0:4", TRUE);
  /* Since "0:2" is hidden, "0:4" must be "0:3" in the filter model */
  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, "0:3", 0);

  set_path_visibility (fixture, "2:0:2", TRUE);
  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, "0:2", LEVEL_LENGTH);
  check_level_length (fixture->filter, "0:3", LEVEL_LENGTH);
  check_level_length (fixture->filter, "0:4", 0);

  set_path_visibility (fixture, "2:0:4:2", TRUE);
  set_path_visibility (fixture, "2:0:4:4", TRUE);
  check_level_length (fixture->filter, "0:4", 2);
}


static void
empty_show_nodes (FilterTest    *fixture,
                  gconstpointer  user_data)
{
  check_filter_model (fixture);
  check_level_length (fixture->filter, NULL, 0);

  set_path_visibility (fixture, "3", TRUE);
  check_filter_model (fixture);
  check_level_length (fixture->filter, NULL, 1);
  check_level_length (fixture->filter, "0", 0);

  set_path_visibility (fixture, "3:2:2", TRUE);
  check_filter_model (fixture);
  check_level_length (fixture->filter, NULL, 1);
  check_level_length (fixture->filter, "0", 0);

  set_path_visibility (fixture, "3:2", TRUE);
  check_filter_model (fixture);
  check_level_length (fixture->filter, NULL, 1);
  check_level_length (fixture->filter, "0", 1);
  check_level_length (fixture->filter, "0:0", 1);
  check_level_length (fixture->filter, "0:0:0", 0);

  set_path_visibility (fixture, "3", FALSE);
  check_filter_model (fixture);
  check_level_length (fixture->filter, NULL, 0);

  set_path_visibility (fixture, "3:2:1", TRUE);
  set_path_visibility (fixture, "3", TRUE);
  check_filter_model (fixture);
  check_level_length (fixture->filter, NULL, 1);
  check_level_length (fixture->filter, "0", 1);
  check_level_length (fixture->filter, "0:0", 2);
  check_level_length (fixture->filter, "0:0:0", 0);
}

static void
empty_vroot_show_nodes (FilterTest    *fixture,
                        gconstpointer  user_data)
{
  GtkTreePath *path = (GtkTreePath *)user_data;

  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, NULL, 0);

  set_path_visibility (fixture, "2", TRUE);
  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, NULL, 0);

  set_path_visibility (fixture, "2:2:2", TRUE);
  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, NULL, 0);

  set_path_visibility (fixture, "2:2", TRUE);
  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, NULL, 1);
  check_level_length (fixture->filter, "0", 1);
  check_level_length (fixture->filter, "0:0", 0);

  set_path_visibility (fixture, "3", TRUE);
  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, NULL, 1);

  set_path_visibility (fixture, "2:2", FALSE);
  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, NULL, 0);

  set_path_visibility (fixture, "2:2:1", TRUE);
  set_path_visibility (fixture, "2:2", TRUE);
  check_filter_model_with_root (fixture, path);
  check_level_length (fixture->filter, NULL, 1);
  check_level_length (fixture->filter, "0", 2);
  check_level_length (fixture->filter, "0:1", 0);
}


static void
unfiltered_hide_single (FilterTest    *fixture,
                        gconstpointer  user_data)

{
  set_path_visibility (fixture, "2", FALSE);

  check_level_length (fixture->filter, NULL, LEVEL_LENGTH);

  filter_test_enable_filter (fixture);

  check_filter_model (fixture);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH - 1);
}

static void
unfiltered_hide_single_child (FilterTest    *fixture,
                              gconstpointer  user_data)

{
  set_path_visibility (fixture, "2:2", FALSE);

  check_level_length (fixture->filter, NULL, LEVEL_LENGTH);
  check_level_length (fixture->filter, "2", LEVEL_LENGTH);

  filter_test_enable_filter (fixture);

  check_filter_model (fixture);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH);
  check_level_length (fixture->filter, "2", LEVEL_LENGTH - 1);
}

static void
unfiltered_hide_single_multi_level (FilterTest    *fixture,
                                    gconstpointer  user_data)

{
  set_path_visibility (fixture, "2:2:2", FALSE);
  set_path_visibility (fixture, "2:2", FALSE);

  check_level_length (fixture->filter, NULL, LEVEL_LENGTH);
  check_level_length (fixture->filter, "2", LEVEL_LENGTH);
  check_level_length (fixture->filter, "2:2", LEVEL_LENGTH);

  filter_test_enable_filter (fixture);

  check_filter_model (fixture);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH);
  check_level_length (fixture->filter, "2", LEVEL_LENGTH - 1);

  set_path_visibility (fixture, "2:2", TRUE);

  check_filter_model (fixture);
  check_level_length (fixture->filter, NULL, LEVEL_LENGTH);
  check_level_length (fixture->filter, "2", LEVEL_LENGTH);
  check_level_length (fixture->filter, "2:2", LEVEL_LENGTH - 1);
}


static void
unfiltered_show_single (FilterTest    *fixture,
                        gconstpointer  user_data)

{
  set_path_visibility (fixture, "2", TRUE);

  check_level_length (fixture->filter, NULL, LEVEL_LENGTH);

  filter_test_enable_filter (fixture);

  check_filter_model (fixture);
  check_level_length (fixture->filter, NULL, 1);
}

static void
unfiltered_show_single_child (FilterTest    *fixture,
                              gconstpointer  user_data)

{
  set_path_visibility (fixture, "2:2", TRUE);

  check_level_length (fixture->filter, NULL, LEVEL_LENGTH);
  check_level_length (fixture->filter, "2", LEVEL_LENGTH);

  filter_test_enable_filter (fixture);

  check_filter_model (fixture);
  check_level_length (fixture->filter, NULL, 0);

  /* From here we are filtered, "2" in the real model is "0" in the filter
   * model.
   */
  set_path_visibility (fixture, "2", TRUE);
  check_level_length (fixture->filter, NULL, 1);
  check_level_length (fixture->filter, "0", 1);
}

static void
unfiltered_show_single_multi_level (FilterTest    *fixture,
                                    gconstpointer  user_data)

{
  set_path_visibility (fixture, "2:2:2", TRUE);
  set_path_visibility (fixture, "2:2", TRUE);

  check_level_length (fixture->filter, NULL, LEVEL_LENGTH);
  check_level_length (fixture->filter, "2", LEVEL_LENGTH);
  check_level_length (fixture->filter, "2:2", LEVEL_LENGTH);

  filter_test_enable_filter (fixture);

  check_filter_model (fixture);
  check_level_length (fixture->filter, NULL, 0);

  /* From here we are filtered, "2" in the real model is "0" in the filter
   * model.
   */
  set_path_visibility (fixture, "2", TRUE);
  check_filter_model (fixture);
  check_level_length (fixture->filter, NULL, 1);
  check_level_length (fixture->filter, "0", 1);
  check_level_length (fixture->filter, "0:0", 1);
}


static gboolean
specific_path_dependent_filter_func (GtkTreeModel *model,
                                     GtkTreeIter  *iter,
                                     gpointer      data)
{
  GtkTreePath *path;

  path = gtk_tree_model_get_path (model, iter);
  if (gtk_tree_path_get_indices (path)[0] < 4)
    return FALSE;

  return TRUE;
}

static void
specific_path_dependent_filter (void)
{
  int i;
  GtkTreeIter iter;
  GtkListStore *list;
  GtkTreeModel *sort;
  GtkTreeModel *filter;

  list = gtk_list_store_new (1, G_TYPE_INT);
  gtk_list_store_insert_with_values (list, &iter, 0, 0, 1, -1);
  gtk_list_store_insert_with_values (list, &iter, 1, 0, 2, -1);
  gtk_list_store_insert_with_values (list, &iter, 2, 0, 3, -1);
  gtk_list_store_insert_with_values (list, &iter, 3, 0, 4, -1);
  gtk_list_store_insert_with_values (list, &iter, 4, 0, 5, -1);
  gtk_list_store_insert_with_values (list, &iter, 5, 0, 6, -1);
  gtk_list_store_insert_with_values (list, &iter, 6, 0, 7, -1);
  gtk_list_store_insert_with_values (list, &iter, 7, 0, 8, -1);

  sort = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (list));
  filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (sort), NULL);
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filter),
                                          specific_path_dependent_filter_func,
                                          NULL, NULL);

  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort), 0,
                                        GTK_SORT_DESCENDING);

  for (i = 0; i < 4; i++)
    {
      if (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (list), &iter,
                                         NULL, 1))
        gtk_list_store_remove (list, &iter);

      if (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (list), &iter,
                                         NULL, 2))
        gtk_list_store_remove (list, &iter);
    }
}


static gboolean
specific_append_after_collapse_visible_func (GtkTreeModel *model,
                                             GtkTreeIter  *iter,
                                             gpointer      data)
{
  gint number;
  gboolean hide_negative_numbers;

  gtk_tree_model_get (model, iter, 1, &number, -1);
  hide_negative_numbers = GPOINTER_TO_INT (g_object_get_data (data, "private-hide-negative-numbers"));

  return (number >= 0 || !hide_negative_numbers);
}

static void
specific_append_after_collapse (void)
{
  /* This test is based on one of the test cases I found in my
   * old test cases directory.  I unfortunately do not have a record
   * from who this test case originated.  -Kris.
   *
   * General idea:
   * - Construct tree.
   * - Show tree, expand, collapse.
   * - Add a row.
   */

  GtkTreeIter iter;
  GtkTreeIter child_iter;
  GtkTreeIter child_iter2;
  GtkTreePath *append_path;
  GtkTreeStore *store;
  GtkTreeModel *filter;
  GtkTreeModel *sort;

  GtkWidget *window;
  GtkWidget *tree_view;

  store = gtk_tree_store_new (2, G_TYPE_STRING, G_TYPE_INT);

  filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (store), NULL);
  g_object_set_data (G_OBJECT (filter), "private-hide-negative-numbers",
                     GINT_TO_POINTER (FALSE));
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filter),
                                          specific_append_after_collapse_visible_func,
                                          filter, NULL);

  sort = gtk_tree_model_sort_new_with_model (filter);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  tree_view = gtk_tree_view_new_with_model (sort);
  gtk_container_add (GTK_CONTAINER (window), tree_view);
  gtk_widget_realize (tree_view);

  while (gtk_events_pending ())
    gtk_main_iteration ();

  gtk_tree_store_prepend (store, &iter, NULL);
  gtk_tree_store_set (store, &iter,
                      0, "hallo", 1, 1, -1);

  gtk_tree_store_append (store, &child_iter, &iter);
  gtk_tree_store_set (store, &child_iter,
                      0, "toemaar", 1, 1, -1);

  gtk_tree_store_append (store, &child_iter2, &child_iter);
  gtk_tree_store_set (store, &child_iter2,
                      0, "very deep", 1, 1, -1);

  append_path = gtk_tree_model_get_path (GTK_TREE_MODEL (store), &child_iter2);

  gtk_tree_store_append (store, &child_iter, &iter);
  gtk_tree_store_set (store, &child_iter,
                      0, "sja", 1, 1, -1);

  gtk_tree_store_append (store, &child_iter, &iter);
  gtk_tree_store_set (store, &child_iter,
                      0, "some word", 1, -1, -1);

  /* Expand and collapse the tree */
  gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));
  while (gtk_events_pending ())
    gtk_main_iteration ();

  gtk_tree_view_collapse_all (GTK_TREE_VIEW (tree_view));
  while (gtk_events_pending ())
    gtk_main_iteration ();

  /* Add another it */
  g_object_set_data (G_OBJECT (filter), "private-hide-negative-numbers",
                     GINT_TO_POINTER (TRUE));

  if (gtk_tree_model_get_iter (GTK_TREE_MODEL (store), &iter, append_path))
    {
      gtk_tree_store_append (store, &child_iter, &iter);
      gtk_tree_store_set (store, &child_iter,
                          0, "new new new !!", 1, 1, -1);
    }
  gtk_tree_path_free (append_path);

  /* Expand */
  gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));
  while (gtk_events_pending ())
    gtk_main_iteration ();
}


static gint
specific_sort_filter_remove_node_compare_func (GtkTreeModel  *model,
                                               GtkTreeIter   *iter1,
                                               GtkTreeIter   *iter2,
                                               gpointer       data)
{
  return -1;
}

static gboolean
specific_sort_filter_remove_node_visible_func (GtkTreeModel  *model,
                                               GtkTreeIter   *iter,
                                               gpointer       data)
{
  char *item = NULL;

  /* Do reference the model */
  gtk_tree_model_get (model, iter, 0, &item, -1);
  g_free (item);

  return FALSE;
}

static void
specific_sort_filter_remove_node (void)
{
  /* This test is based on one of the test cases I found in my
   * old test cases directory.  I unfortunately do not have a record
   * from who this test case originated.  -Kris.
   *
   * General idea:
   *  - Create tree store, sort, filter models.  The sort model has
   *    a default sort func that is enabled, filter model a visible func
   *    that defaults to returning FALSE.
   *  - Remove a node from the tree store.
   */

  GtkTreeIter iter;
  GtkTreeStore *store;
  GtkTreeModel *filter;
  GtkTreeModel *sort;

  GtkWidget *window;
  GtkWidget *tree_view;

  store = gtk_tree_store_new (1, G_TYPE_STRING);
  gtk_tree_store_append (store, &iter, NULL);
  gtk_tree_store_set (store, &iter, 0, "Hello1", -1);

  gtk_tree_store_append (store, &iter, NULL);
  gtk_tree_store_set (store, &iter, 0, "Hello2", -1);

  sort = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (store));
  gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (sort),
                                           specific_sort_filter_remove_node_compare_func, NULL, NULL);

  filter = gtk_tree_model_filter_new (sort, NULL);
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filter),
                                          specific_sort_filter_remove_node_visible_func,
                                          filter, NULL);


  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  tree_view = gtk_tree_view_new_with_model (filter);
  gtk_container_add (GTK_CONTAINER (window), tree_view);
  gtk_widget_realize (tree_view);

  while (gtk_events_pending ())
    gtk_main_iteration ();

  /* Remove a node */
  gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter);
  gtk_tree_model_iter_next (GTK_TREE_MODEL (store), &iter);
  gtk_tree_store_remove (store, &iter);

  while (gtk_events_pending ())
    gtk_main_iteration ();
}


static void
specific_sort_filter_remove_root (void)
{
  /* This test is based on one of the test cases I found in my
   * old test cases directory.  I unfortunately do not have a record
   * from who this test case originated.  -Kris.
   */

  GtkTreeModel *model, *sort, *filter;
  GtkTreeIter root, mid, leaf;
  GtkTreePath *path;

  model = GTK_TREE_MODEL (gtk_tree_store_new (1, G_TYPE_INT));
  gtk_tree_store_append (GTK_TREE_STORE (model), &root, NULL);
  gtk_tree_store_append (GTK_TREE_STORE (model), &mid, &root);
  gtk_tree_store_append (GTK_TREE_STORE (model), &leaf, &mid);

  path = gtk_tree_model_get_path (model, &mid);

  sort = gtk_tree_model_sort_new_with_model (model);
  filter = gtk_tree_model_filter_new (sort, path);

  gtk_tree_store_remove (GTK_TREE_STORE (model), &root);

  g_object_unref (filter);
  g_object_unref (sort);
  g_object_unref (model);
}


static void
specific_filter_add_child (void)
{
  /* This test is based on one of the test cases I found in my
   * old test cases directory.  I unfortunately do not have a record
   * from who this test case originated.  -Kris.
   */

  GtkTreeIter iter;
  GtkTreeIter iter_first;
  GtkTreeIter child;
  GtkTreeStore *store;
  GtkTreeModel *filter;

  store = gtk_tree_store_new (1, G_TYPE_STRING);

  gtk_tree_store_append (store, &iter_first, NULL);
  gtk_tree_store_set (store, &iter_first, 0, "Hello", -1);

  gtk_tree_store_append (store, &iter, NULL);
  gtk_tree_store_set (store, &iter, 0, "Hello", -1);

  gtk_tree_store_append (store, &iter, NULL);
  gtk_tree_store_set (store, &iter, 0, "Hello", -1);

  gtk_tree_store_append (store, &iter, NULL);
  gtk_tree_store_set (store, &iter, 0, "Hello", -1);

  filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (store), NULL);

  gtk_tree_store_set (store, &iter, 0, "Hello", -1);
  gtk_tree_store_append (store, &child, &iter_first);
  gtk_tree_store_set (store, &child, 0, "Hello", -1);
}


static void
specific_bug_300089 (void)
{
  /* Test case for GNOME Bugzilla bug 300089.  Written by
   * Matthias Clasen.
   */
  GtkTreeModel *sort_model, *child_model;
  GtkTreePath *path;
  GtkTreeIter iter, iter2, sort_iter;

  child_model = GTK_TREE_MODEL (gtk_tree_store_new (1, G_TYPE_STRING));

  gtk_tree_store_append (GTK_TREE_STORE (child_model), &iter, NULL);
  gtk_tree_store_set (GTK_TREE_STORE (child_model), &iter, 0, "A", -1);
  gtk_tree_store_append (GTK_TREE_STORE (child_model), &iter, NULL);
  gtk_tree_store_set (GTK_TREE_STORE (child_model), &iter, 0, "B", -1);

  gtk_tree_store_append (GTK_TREE_STORE (child_model), &iter2, &iter);
  gtk_tree_store_set (GTK_TREE_STORE (child_model), &iter2, 0, "D", -1);
  gtk_tree_store_append (GTK_TREE_STORE (child_model), &iter2, &iter);
  gtk_tree_store_set (GTK_TREE_STORE (child_model), &iter2, 0, "E", -1);

  gtk_tree_store_append (GTK_TREE_STORE (child_model), &iter, NULL);
  gtk_tree_store_set (GTK_TREE_STORE (child_model), &iter, 0, "C", -1);


  sort_model = GTK_TREE_MODEL (gtk_tree_model_sort_new_with_model (child_model));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (sort_model),
                                        0, GTK_SORT_ASCENDING);

  path = gtk_tree_path_new_from_indices (1, 1, -1);

  /* make sure a level is constructed */ 
  gtk_tree_model_get_iter (sort_model, &sort_iter, path);

  /* change the "E" row in a way that causes it to change position */ 
  gtk_tree_model_get_iter (child_model, &iter, path);
  gtk_tree_store_set (GTK_TREE_STORE (child_model), &iter, 0, "A", -1);
}


static int
specific_bug_301558_sort_func (GtkTreeModel *model,
                               GtkTreeIter  *a,
                               GtkTreeIter  *b,
                               gpointer      data)
{
  int i, j;

  gtk_tree_model_get (model, a, 0, &i, -1);
  gtk_tree_model_get (model, b, 0, &j, -1);

  return j - i;
}

static void
specific_bug_301558 (void)
{
  /* Test case for GNOME Bugzilla bug 301558 provided by
   * Markku Vire.
   */
  GtkTreeStore *tree;
  GtkTreeModel *filter;
  GtkTreeModel *sort;
  GtkTreeIter root, iter, iter2;
  GtkWidget *view;
  int i;
  gboolean add;

  tree = gtk_tree_store_new (2, G_TYPE_INT, G_TYPE_BOOLEAN);
  gtk_tree_store_append (tree, &iter, NULL);
  gtk_tree_store_set (tree, &iter, 0, 123, 1, TRUE, -1);
  gtk_tree_store_append (tree, &iter2, &iter);
  gtk_tree_store_set (tree, &iter2, 0, 73, 1, TRUE, -1);

  sort = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (tree));
  gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (sort),
                                           specific_bug_301558_sort_func,
                                           NULL, NULL);

  filter = gtk_tree_model_filter_new (sort, NULL);
  gtk_tree_model_filter_set_visible_column (GTK_TREE_MODEL_FILTER (filter), 1);

  view = gtk_tree_view_new_with_model (filter);

  while (gtk_events_pending ())
    gtk_main_iteration ();

  add = TRUE;

  for (i = 0; i < 10; i++)
    {
      if (!gtk_tree_model_get_iter_first (GTK_TREE_MODEL (tree), &root))
        g_assert_not_reached ();

      if (add)
        {
          gtk_tree_store_append (tree, &iter, &root);
          gtk_tree_store_set (tree, &iter, 0, 456, 1, TRUE, -1);
        }
      else
        {
          int n;
          n = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (tree), &root);
          gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (tree), &iter,
                                         &root, n - 1);
          gtk_tree_store_remove (tree, &iter);
        }

      add = !add;
    }
}


static gboolean
specific_bug_311955_filter_func (GtkTreeModel *model,
                                 GtkTreeIter  *iter,
                                 gpointer      data)
{
  int value;

  gtk_tree_model_get (model, iter, 0, &value, -1);

  return (value != 0);
}

static void
specific_bug_311955 (void)
{
  /* This is a test case for GNOME Bugzilla bug 311955.  It was written
   * by Markku Vire.
   */
  GtkTreeIter iter, child, root;
  GtkTreeStore *store;
  GtkTreeModel *sort;
  GtkTreeModel *filter;

  GtkWidget *window;
  GtkWidget *tree_view;
  int i;
  int n;

  store = gtk_tree_store_new (1, G_TYPE_INT);

  gtk_tree_store_append (store, &root, NULL);
  gtk_tree_store_set (store, &root, 0, 33, -1);

  gtk_tree_store_append (store, &iter, &root);
  gtk_tree_store_set (store, &iter, 0, 50, -1);

  gtk_tree_store_append (store, &iter, NULL);
  gtk_tree_store_set (store, &iter, 0, 22, -1);

  sort = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (store));
  filter = gtk_tree_model_filter_new (sort, NULL);

  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filter),
                                          specific_bug_311955_filter_func,
                                          NULL, NULL);

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  tree_view = gtk_tree_view_new_with_model (filter);
  g_object_unref (store);

  gtk_tree_view_expand_all (GTK_TREE_VIEW (tree_view));

  while (gtk_events_pending ())
    gtk_main_iteration ();

  /* Fill model */
  for (i = 0; i < 4; i++)
    {
      gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &root);

      gtk_tree_store_append (store, &iter, &root);

      if (i < 3)
        gtk_tree_store_set (store, &iter, 0, i, -1);

      if (i % 2 == 0)
        {
          gtk_tree_store_append (store, &child, &iter);
          gtk_tree_store_set (store, &child, 0, 10, -1);
        }
    }

  while (gtk_events_pending ())
    gtk_main_iteration ();

  /* Remove bottommost child from the tree. */
  gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &root);
  n = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (store), &root);

  if (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (store), &iter, &root, n - 2))
    {
      if (gtk_tree_model_iter_children (GTK_TREE_MODEL (store), &child, &iter))
        gtk_tree_store_remove (store, &child);
    }
  else
    g_assert_not_reached ();
}

static void
specific_bug_346800 (void)
{
  /* This is a test case for GNOME Bugzilla bug 346800.  It was written
   * by Jonathan Matthew.
   */

  GtkTreeIter node_iters[50];
  GtkTreeIter child_iters[50];
  GtkTreeModel *model;
  GtkTreeModelFilter *filter;
  GtkTreeStore *store;
  GType *columns;
  int i;
  int items = 50;
  columns = g_new (GType, 2);
  columns[0] = G_TYPE_STRING;
  columns[1] = G_TYPE_BOOLEAN;
  store = gtk_tree_store_newv (2, columns);
  model = GTK_TREE_MODEL (store);

  filter = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (model, NULL));
  gtk_tree_model_filter_set_visible_column (filter, 1);

  for (i=0; i<items; i++)
    {
      /* allocate random amounts of junk, otherwise the filter model's arrays can expand without moving */

      g_malloc (138);
      gtk_tree_store_append (store, &node_iters[i], NULL);
      gtk_tree_store_set (store, &node_iters[i],
                          0, "something",
                          1, ((i%6) == 0) ? FALSE : TRUE,
                          -1);

      g_malloc (47);
      gtk_tree_store_append (store, &child_iters[i], &node_iters[i]);
      gtk_tree_store_set (store, &child_iters[i],
                          0, "something else",
                          1, FALSE,
                          -1);
      gtk_tree_model_filter_refilter (filter);

      if (i > 6)
        {
          gtk_tree_store_set (GTK_TREE_STORE (model), &child_iters[i-1], 1,
                              (i & 1) ? TRUE : FALSE, -1);
          gtk_tree_model_filter_refilter (filter);

          gtk_tree_store_set (GTK_TREE_STORE (model), &child_iters[i-2], 1,
                              (i & 1) ? FALSE: TRUE, -1);
          gtk_tree_model_filter_refilter (filter);
        }
    }
}


static void
specific_bug_364946 (void)
{
  /* This is a test case for GNOME Bugzilla bug 364946.  It was written
   * by Andreas Koehler.
   */
  GtkTreeStore *store;
  GtkTreeIter a, aa, aaa, aab, iter;
  GtkTreeModel *s_model;

  store = gtk_tree_store_new (1, G_TYPE_STRING);

  gtk_tree_store_append (store, &a, NULL);
  gtk_tree_store_set (store, &a, 0, "0", -1);

  gtk_tree_store_append (store, &aa, &a);
  gtk_tree_store_set (store, &aa, 0, "0:0", -1);

  gtk_tree_store_append (store, &aaa, &aa);
  gtk_tree_store_set (store, &aaa, 0, "0:0:0", -1);

  gtk_tree_store_append (store, &aab, &aa);
  gtk_tree_store_set (store, &aab, 0, "0:0:1", -1);

  s_model = gtk_tree_model_sort_new_with_model (GTK_TREE_MODEL (store));
  gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (s_model), 0,
                                        GTK_SORT_ASCENDING);

  gtk_tree_model_get_iter_from_string (s_model, &iter, "0:0:0");

  gtk_tree_store_set (store, &aaa, 0, "0:0:0", -1);
  gtk_tree_store_remove (store, &aaa);
  gtk_tree_store_remove (store, &aab);

  gtk_tree_model_sort_clear_cache (GTK_TREE_MODEL_SORT (s_model));
}


static gboolean
specific_bug_464173_visible_func (GtkTreeModel *model,
                                  GtkTreeIter  *iter,
                                  gpointer      data)
{
  gboolean *visible = (gboolean *)data;

  return *visible;
}

static void
specific_bug_464173 (void)
{
  /* Test case for GNOME Bugzilla bug 464173, test case written
   * by Andreas Koehler.
   */
  GtkTreeStore *model;
  GtkTreeModelFilter *f_model;
  GtkTreeIter iter1, iter2;
  GtkWidget *view;
  gboolean visible = TRUE;

  model = gtk_tree_store_new (1, G_TYPE_STRING);
  gtk_tree_store_append (model, &iter1, NULL);
  gtk_tree_store_set (model, &iter1, 0, "Foo", -1);
  gtk_tree_store_append (model, &iter2, &iter1);
  gtk_tree_store_set (model, &iter2, 0, "Bar", -1);

  f_model = GTK_TREE_MODEL_FILTER (gtk_tree_model_filter_new (GTK_TREE_MODEL(model), NULL));
  gtk_tree_model_filter_set_visible_func (f_model,
                                          specific_bug_464173_visible_func,
                                          &visible, NULL);

  view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (f_model));

  visible = FALSE;
  gtk_tree_model_filter_refilter (f_model);
}


static gboolean
specific_bug_540201_filter_func (GtkTreeModel *model,
                                 GtkTreeIter  *iter,
                                 gpointer      data)
{
  gboolean has_children;

  has_children = gtk_tree_model_iter_has_child (model, iter);

  return has_children;
}

static void
specific_bug_540201 (void)
{
  /* Test case for GNOME Bugzilla bug 540201, steps provided by
   * Charles Day.
   */
  GtkTreeIter iter, root;
  GtkTreeStore *store;
  GtkTreeModel *filter;

  GtkWidget *tree_view;

  store = gtk_tree_store_new (1, G_TYPE_INT);

  gtk_tree_store_append (store, &root, NULL);
  gtk_tree_store_set (store, &root, 0, 33, -1);

  filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (store), NULL);
  tree_view = gtk_tree_view_new_with_model (filter);

  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filter),
                                          specific_bug_540201_filter_func,
                                          NULL, NULL);

  gtk_tree_store_append (store, &iter, &root);
  gtk_tree_store_set (store, &iter, 0, 50, -1);

  gtk_tree_store_append (store, &iter, &root);
  gtk_tree_store_set (store, &iter, 0, 22, -1);


  gtk_tree_store_append (store, &root, NULL);
  gtk_tree_store_set (store, &root, 0, 33, -1);

  gtk_tree_store_append (store, &iter, &root);
  gtk_tree_store_set (store, &iter, 0, 22, -1);
}


static gboolean
specific_bug_549287_visible_func (GtkTreeModel *model,
                                  GtkTreeIter  *iter,
                                  gpointer      data)
{
  gboolean result = FALSE;

  result = gtk_tree_model_iter_has_child (model, iter);

  return result;
}

static void
specific_bug_549287 (void)
{
  /* Test case for GNOME Bugzilla bug 529287, provided by Julient Puydt */

  int i;
  GtkTreeStore *store;
  GtkTreeModel *filtered;
  GtkWidget *view;
  GtkTreeIter iter;
  GtkTreeIter *swap, *parent, *child;

  store = gtk_tree_store_new (1, G_TYPE_STRING);
  filtered = gtk_tree_model_filter_new (GTK_TREE_MODEL (store), NULL);
  gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (filtered),
                                          specific_bug_549287_visible_func,
                                          NULL, NULL);

  view = gtk_tree_view_new_with_model (filtered);

  for (i = 0; i < 4; i++)
    {
      if (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (store), &iter))
        {
          parent = gtk_tree_iter_copy (&iter);
          child = gtk_tree_iter_copy (&iter);

          while (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (store),
                                                child, parent, 0))
            {

              swap = parent;
              parent = child;
              child = swap;
            }

          gtk_tree_store_append (store, child, parent);
          gtk_tree_store_set (store, child,
                              0, "Something",
                              -1);

          gtk_tree_iter_free (parent);
          gtk_tree_iter_free (child);
        }
      else
        {
          gtk_tree_store_append (store, &iter, NULL);
          gtk_tree_store_set (store, &iter,
                              0, "Something",
                              -1);
        }

      /* since we inserted something, we changed the visibility conditions: */
      gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (filtered));
    }
}

/* main */

int
main (int    argc,
      char **argv)
{
  gtk_test_init (&argc, &argv, NULL);

  g_test_add ("/FilterModel/self/verify-test-suite",
              FilterTest, NULL,
              filter_test_setup,
              verify_test_suite,
              filter_test_teardown);

  g_test_add ("/FilterModel/self/verify-test-suite/vroot/depth-1",
              FilterTest, gtk_tree_path_new_from_indices (2, -1),
              filter_test_setup,
              verify_test_suite_vroot,
              filter_test_teardown);
  g_test_add ("/FilterModel/self/verify-test-suite/vroot/depth-2",
              FilterTest, gtk_tree_path_new_from_indices (2, 3, -1),
              filter_test_setup,
              verify_test_suite_vroot,
              filter_test_teardown);


  g_test_add ("/FilterModel/filled/hide-root-level",
              FilterTest, NULL,
              filter_test_setup,
              filled_hide_root_level,
              filter_test_teardown);
  g_test_add ("/FilterModel/filled/hide-child-levels",
              FilterTest, NULL,
              filter_test_setup,
              filled_hide_child_levels,
              filter_test_teardown);

  g_test_add ("/FilterModel/filled/hide-root-level/vroot",
              FilterTest, gtk_tree_path_new_from_indices (2, -1),
              filter_test_setup,
              filled_vroot_hide_root_level,
              filter_test_teardown);
  g_test_add ("/FilterModel/filled/hide-child-levels/vroot",
              FilterTest, gtk_tree_path_new_from_indices (2, -1),
              filter_test_setup,
              filled_vroot_hide_child_levels,
              filter_test_teardown);


  g_test_add ("/FilterModel/empty/show-nodes",
              FilterTest, NULL,
              filter_test_setup_empty,
              empty_show_nodes,
              filter_test_teardown);

  g_test_add ("/FilterModel/empty/show-nodes/vroot",
              FilterTest, gtk_tree_path_new_from_indices (2, -1),
              filter_test_setup_empty,
              empty_vroot_show_nodes,
              filter_test_teardown);


  g_test_add ("/FilterModel/unfiltered/hide-single",
              FilterTest, NULL,
              filter_test_setup_unfiltered,
              unfiltered_hide_single,
              filter_test_teardown);
  g_test_add ("/FilterModel/unfiltered/hide-single-child",
              FilterTest, NULL,
              filter_test_setup_unfiltered,
              unfiltered_hide_single_child,
              filter_test_teardown);
  g_test_add ("/FilterModel/unfiltered/hide-single-multi-level",
              FilterTest, NULL,
              filter_test_setup_unfiltered,
              unfiltered_hide_single_multi_level,
              filter_test_teardown);

  g_test_add ("/FilterModel/unfiltered/show-single",
              FilterTest, NULL,
              filter_test_setup_empty_unfiltered,
              unfiltered_show_single,
              filter_test_teardown);
  g_test_add ("/FilterModel/unfiltered/show-single-child",
              FilterTest, NULL,
              filter_test_setup_empty_unfiltered,
              unfiltered_show_single_child,
              filter_test_teardown);
  g_test_add ("/FilterModel/unfiltered/show-single-multi-level",
              FilterTest, NULL,
              filter_test_setup_empty_unfiltered,
              unfiltered_show_single_multi_level,
              filter_test_teardown);


  g_test_add_func ("/FilterModel/specific/path-dependent-filter",
                   specific_path_dependent_filter);
  g_test_add_func ("/FilterModel/specific/append-after-collapse",
                   specific_append_after_collapse);
  g_test_add_func ("/FilterModel/specific/sort-filter-remove-node",
                   specific_sort_filter_remove_node);
  g_test_add_func ("/FilterModel/specific/sort-filter-remove-root",
                   specific_sort_filter_remove_root);
  g_test_add_func ("/FilterModel/specific/filter-add-child",
                   specific_filter_add_child);

  g_test_add_func ("/FilterModel/specific/bug-300089",
                   specific_bug_300089);
  g_test_add_func ("/FilterModel/specific/bug-301558",
                   specific_bug_301558);
  g_test_add_func ("/FilterModel/specific/bug-311955",
                   specific_bug_311955);
  g_test_add_func ("/FilterModel/specific/bug-346800",
                   specific_bug_346800);
  g_test_add_func ("/FilterModel/specific/bug-364946",
                   specific_bug_364946);
  g_test_add_func ("/FilterModel/specific/bug-464173",
                   specific_bug_464173);
  g_test_add_func ("/FilterModel/specific/bug-540201",
                   specific_bug_540201);
  g_test_add_func ("/FilterModel/specific/bug-549287",
                   specific_bug_549287);

  return g_test_run ();
}