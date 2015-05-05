package com.broadcom.tvinput;

import java.util.Collection;
import java.util.HashMap;
import java.util.Map;
import java.util.Set;

/**
 * @brief Simple bidirectional map.
 */
public class BiMap<K, V> implements Map<K, V> {
    private HashMap<K, V> forward;
    private HashMap<V, K> reverse;

    public BiMap() {
        forward = new HashMap<K, V>();
        reverse = new HashMap<V, K>();
    }

    public BiMap(int capacity) {
        forward = new HashMap<K, V>(capacity);
        reverse = new HashMap<V, K>(capacity);
    }

    @Override
    public void clear() {
        forward.clear();
        reverse.clear();
    }

    @Override
    public boolean containsKey(Object key) {
        return forward.containsKey(key);
    }

    @Override
    public boolean containsValue(Object value) {
        //return forward.containsValue(value); //likely to be slower
        return reverse.containsKey(value); //likely to be faster
    }

    @Override
    public V get(Object key) {
        return forward.get(key);
    }

    // NOT @Override
    public K rget(Object value) {
        return reverse.get(value);
    }

    @Override
    public boolean isEmpty() {
        return forward.isEmpty();
    }

    @Override
    public V put(K key, V value) {
        if (reverse.containsKey(value) && !reverse.get(value).equals(key)) {
            throw new IllegalArgumentException("non-unique value");
        }

        //add new forward mapping and get old reverse map key, if any
        V old_value = forward.put(key, value);

        //delete old reverse mapping, if present
        if (/*old_value != null &&*/ reverse.containsKey(old_value)) {
            reverse.remove(old_value);
        }

        //add new reverse mapping
        reverse.put(value, key);

        return old_value;
    }

    @Override
    public V remove(Object key) {
        V value = forward.remove(key);
        reverse.remove(value);
        return value;
    }

    @Override
    public int size() {
        return forward.size();
    }

    @Override
    public Set<java.util.Map.Entry<K, V>> entrySet() {
        throw new UnsupportedOperationException();
    }

    @Override
    public Set<K> keySet() {
        throw new UnsupportedOperationException();
    }

    @Override
    public void putAll(Map<? extends K, ? extends V> map) {
        throw new UnsupportedOperationException();
    }

    @Override
    public Collection<V> values() {
        throw new UnsupportedOperationException();
    }

}
