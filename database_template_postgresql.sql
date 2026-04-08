--
-- PostgreSQL database dump
--

\restrict rzOHDTlPZOgdAw3ORALnep3Wa79RscPRrC7SPgDjcpC6ehjfX13iW2uwGRmg5kA

-- Dumped from database version 17.7 (Debian 17.7-0+deb13u1)
-- Dumped by pg_dump version 17.7 (Debian 17.7-0+deb13u1)

SET statement_timeout = 0;
SET lock_timeout = 0;
SET idle_in_transaction_session_timeout = 0;
SET transaction_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = on;
SELECT pg_catalog.set_config('search_path', '', false);
SET check_function_bodies = false;
SET xmloption = content;
SET client_min_messages = warning;
SET row_security = off;

--
-- Name: pgcrypto; Type: EXTENSION; Schema: -; Owner: -
--

CREATE EXTENSION IF NOT EXISTS pgcrypto WITH SCHEMA public;


--
-- Name: EXTENSION pgcrypto; Type: COMMENT; Schema: -; Owner: 
--

COMMENT ON EXTENSION pgcrypto IS 'cryptographic functions';


SET default_tablespace = '';

SET default_table_access_method = heap;

--
-- Name: account; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.account (
    username text,
    password_hash text,
    created_at timestamp without time zone,
    last_logged_in timestamp without time zone,
    administrator boolean DEFAULT false,
    key text,
    id integer
);


ALTER TABLE public.account OWNER TO postgres;

--
-- Name: account_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.account_id_seq
    START WITH 4
    INCREMENT BY 1
    MINVALUE 4
    NO MAXVALUE
    CACHE 1;


ALTER SEQUENCE public.account_id_seq OWNER TO postgres;

--
-- Name: permission_collection; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.permission_collection (
    id integer,
    permission_object_id integer,
    account_id integer DEFAULT '-1'::integer,
    permission_group_id integer DEFAULT '-1'::integer
);


ALTER TABLE public.permission_collection OWNER TO postgres;

--
-- Name: permission_collection_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.permission_collection_id_seq
    START WITH 100
    INCREMENT BY 1
    MINVALUE 100
    NO MAXVALUE
    CACHE 1;


ALTER SEQUENCE public.permission_collection_id_seq OWNER TO postgres;

--
-- Name: permission_group; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.permission_group (
    id integer,
    name text
);


ALTER TABLE public.permission_group OWNER TO postgres;

--
-- Name: permission_group_account; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.permission_group_account (
    group_id integer,
    account_id integer
);


ALTER TABLE public.permission_group_account OWNER TO postgres;

--
-- Name: permission_group_heirarchy; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.permission_group_heirarchy (
    rank integer,
    permission_group integer
);


ALTER TABLE public.permission_group_heirarchy OWNER TO postgres;

--
-- Name: permission_group_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.permission_group_id_seq
    START WITH 3
    INCREMENT BY 1
    MINVALUE 3
    NO MAXVALUE
    CACHE 1;


ALTER SEQUENCE public.permission_group_id_seq OWNER TO postgres;

--
-- Name: permission_object_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.permission_object_id_seq
    START WITH 2
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER SEQUENCE public.permission_object_id_seq OWNER TO postgres;

--
-- Name: permission_setting; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.permission_setting (
    id integer,
    permission_collection_id integer,
    permission_number integer,
    setting integer
);


ALTER TABLE public.permission_setting OWNER TO postgres;

--
-- Name: permission_setting_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.permission_setting_id_seq
    START WITH 100
    INCREMENT BY 1
    MINVALUE 100
    NO MAXVALUE
    CACHE 1;


ALTER SEQUENCE public.permission_setting_id_seq OWNER TO postgres;

--
-- Name: post; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.post (
    id integer,
    name text,
    upload_timestamp timestamp without time zone,
    content text,
    files text[],
    thread integer,
    id_in_thread integer,
    key text,
    deleted boolean DEFAULT false
);


ALTER TABLE public.post OWNER TO postgres;

--
-- Name: post_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.post_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER SEQUENCE public.post_id_seq OWNER TO postgres;

--
-- Name: test; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.test (
    nextval bigint
);


ALTER TABLE public.test OWNER TO postgres;

--
-- Name: thread; Type: TABLE; Schema: public; Owner: postgres
--

CREATE TABLE public.thread (
    id integer,
    deleted boolean DEFAULT false,
    permission_object_id integer
);


ALTER TABLE public.thread OWNER TO postgres;

--
-- Name: thread_id_seq; Type: SEQUENCE; Schema: public; Owner: postgres
--

CREATE SEQUENCE public.thread_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER SEQUENCE public.thread_id_seq OWNER TO postgres;

--
-- Name: account account_id_unique; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.account
    ADD CONSTRAINT account_id_unique UNIQUE (id);


--
-- Name: permission_collection permission_collection_permission_object_id_account_id_permi_key; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.permission_collection
    ADD CONSTRAINT permission_collection_permission_object_id_account_id_permi_key UNIQUE (permission_object_id, account_id, permission_group_id);


--
-- Name: permission_collection permission_collection_unique_id; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.permission_collection
    ADD CONSTRAINT permission_collection_unique_id UNIQUE (id);


--
-- Name: permission_group_heirarchy permission_group_heirarchy_permission_group_key; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.permission_group_heirarchy
    ADD CONSTRAINT permission_group_heirarchy_permission_group_key UNIQUE (permission_group);


--
-- Name: permission_group_heirarchy permission_group_heirarchy_rank_key; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.permission_group_heirarchy
    ADD CONSTRAINT permission_group_heirarchy_rank_key UNIQUE (rank);


--
-- Name: permission_group_heirarchy permission_group_heirarchy_rank_permission_group_key; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.permission_group_heirarchy
    ADD CONSTRAINT permission_group_heirarchy_rank_permission_group_key UNIQUE (rank, permission_group);


--
-- Name: permission_group permission_object_unique_id; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.permission_group
    ADD CONSTRAINT permission_object_unique_id UNIQUE (id);


--
-- Name: permission_setting permission_setting_id_key; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.permission_setting
    ADD CONSTRAINT permission_setting_id_key UNIQUE (id);


--
-- Name: permission_setting permission_setting_permission_collection_id_permission_numb_key; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.permission_setting
    ADD CONSTRAINT permission_setting_permission_collection_id_permission_numb_key UNIQUE (permission_collection_id, permission_number);


--
-- Name: thread thread_id_key; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.thread
    ADD CONSTRAINT thread_id_key UNIQUE (id);


--
-- Name: permission_group unique_id; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.permission_group
    ADD CONSTRAINT unique_id UNIQUE (id);


--
-- Name: account username_unique; Type: CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.account
    ADD CONSTRAINT username_unique UNIQUE (username);


--
-- Name: permission_group_account permission_group_account_account_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.permission_group_account
    ADD CONSTRAINT permission_group_account_account_id_fkey FOREIGN KEY (account_id) REFERENCES public.account(id);


--
-- Name: permission_group_account permission_group_account_group_id_fkey; Type: FK CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.permission_group_account
    ADD CONSTRAINT permission_group_account_group_id_fkey FOREIGN KEY (group_id) REFERENCES public.permission_group(id);


--
-- Name: permission_group_heirarchy permission_group_heirarchy_permission_group_fkey; Type: FK CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.permission_group_heirarchy
    ADD CONSTRAINT permission_group_heirarchy_permission_group_fkey FOREIGN KEY (permission_group) REFERENCES public.permission_group(id);


--
-- Name: post post_thread_fkey; Type: FK CONSTRAINT; Schema: public; Owner: postgres
--

ALTER TABLE ONLY public.post
    ADD CONSTRAINT post_thread_fkey FOREIGN KEY (thread) REFERENCES public.thread(id);


--
-- Name: TABLE account; Type: ACL; Schema: public; Owner: postgres
--

GRANT SELECT,INSERT,REFERENCES,DELETE,TRIGGER,TRUNCATE,UPDATE ON TABLE public.account TO mediaboard_server;


--
-- Name: SEQUENCE account_id_seq; Type: ACL; Schema: public; Owner: postgres
--

GRANT SELECT,USAGE ON SEQUENCE public.account_id_seq TO mediaboard_server;


--
-- Name: TABLE permission_collection; Type: ACL; Schema: public; Owner: postgres
--

GRANT SELECT,INSERT,REFERENCES,DELETE,TRIGGER,TRUNCATE,UPDATE ON TABLE public.permission_collection TO mediaboard_server;


--
-- Name: SEQUENCE permission_collection_id_seq; Type: ACL; Schema: public; Owner: postgres
--

GRANT ALL ON SEQUENCE public.permission_collection_id_seq TO mediaboard_server;


--
-- Name: TABLE permission_group; Type: ACL; Schema: public; Owner: postgres
--

GRANT SELECT,INSERT,REFERENCES,DELETE,TRIGGER,TRUNCATE,UPDATE ON TABLE public.permission_group TO mediaboard_server;


--
-- Name: TABLE permission_group_account; Type: ACL; Schema: public; Owner: postgres
--

GRANT SELECT,INSERT,REFERENCES,DELETE,TRIGGER,TRUNCATE,UPDATE ON TABLE public.permission_group_account TO mediaboard_server;


--
-- Name: TABLE permission_group_heirarchy; Type: ACL; Schema: public; Owner: postgres
--

GRANT SELECT,INSERT,REFERENCES,DELETE,TRIGGER,TRUNCATE,UPDATE ON TABLE public.permission_group_heirarchy TO mediaboard_server;


--
-- Name: SEQUENCE permission_group_id_seq; Type: ACL; Schema: public; Owner: postgres
--

GRANT ALL ON SEQUENCE public.permission_group_id_seq TO mediaboard_server;


--
-- Name: SEQUENCE permission_object_id_seq; Type: ACL; Schema: public; Owner: postgres
--

GRANT ALL ON SEQUENCE public.permission_object_id_seq TO mediaboard_server;


--
-- Name: TABLE permission_setting; Type: ACL; Schema: public; Owner: postgres
--

GRANT SELECT,INSERT,REFERENCES,DELETE,TRIGGER,TRUNCATE,UPDATE ON TABLE public.permission_setting TO mediaboard_server;


--
-- Name: SEQUENCE permission_setting_id_seq; Type: ACL; Schema: public; Owner: postgres
--

GRANT ALL ON SEQUENCE public.permission_setting_id_seq TO mediaboard_server;


--
-- Name: TABLE post; Type: ACL; Schema: public; Owner: postgres
--

GRANT SELECT,INSERT,UPDATE ON TABLE public.post TO mediaboard_server;


--
-- Name: SEQUENCE post_id_seq; Type: ACL; Schema: public; Owner: postgres
--

GRANT SELECT,USAGE ON SEQUENCE public.post_id_seq TO mediaboard_server;


--
-- Name: TABLE thread; Type: ACL; Schema: public; Owner: postgres
--

GRANT SELECT,INSERT,UPDATE ON TABLE public.thread TO mediaboard_server;


--
-- Name: SEQUENCE thread_id_seq; Type: ACL; Schema: public; Owner: postgres
--

GRANT SELECT,USAGE ON SEQUENCE public.thread_id_seq TO mediaboard_server;


--
-- PostgreSQL database dump complete
--

\unrestrict rzOHDTlPZOgdAw3ORALnep3Wa79RscPRrC7SPgDjcpC6ehjfX13iW2uwGRmg5kA

